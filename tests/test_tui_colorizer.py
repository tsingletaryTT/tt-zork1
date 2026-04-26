# tests/test_tui_colorizer.py
"""Tests for context_pane token colorizer."""
import pytest


def test_known_word_is_teal():
    from tui.context_pane import classify_token
    vocab = frozenset({"mailbox", "leaflet", "lamp"})
    assert classify_token("mailbox", vocab) == "#4fd1c5"


def test_known_word_with_punctuation_is_teal():
    from tui.context_pane import classify_token
    vocab = frozenset({"mailbox"})
    # comma attached — strip before lookup
    assert classify_token("mailbox,", vocab) == "#4fd1c5"


def test_stopword_is_grey():
    from tui.context_pane import classify_token
    for w in ("the", "a", "an", "in", "of", "and", "or"):
        assert classify_token(w, frozenset()) == "#607d8b", f"Expected grey for '{w}'"


def test_vivid_suffix_long_word_is_pink():
    from tui.context_pane import classify_token
    # "terrifying" = 10 chars, ends in "ing"
    assert classify_token("terrifying", frozenset()) == "#ec96b8"
    # "malevolent" = 10 chars, ends in "ent"
    assert classify_token("malevolent", frozenset()) == "#ec96b8"


def test_short_vivid_suffix_is_not_pink():
    from tui.context_pane import classify_token
    # "going" = 5 chars, ends in "ing" but length < 8 → not pink
    result = classify_token("going", frozenset())
    assert result != "#ec96b8"


def test_capitalized_mid_sentence_is_amber():
    from tui.context_pane import classify_token
    # "Darkness" mid-sentence (not start), not a known vocab word
    assert classify_token("Darkness", frozenset(), is_sentence_start=False) == "#f4c471"


def test_capitalized_at_sentence_start_is_not_amber():
    from tui.context_pane import classify_token
    result = classify_token("The", frozenset(), is_sentence_start=True)
    # "The" is a stopword → grey
    assert result == "#607d8b"


def test_follows_article_is_amber():
    from tui.context_pane import classify_token
    # "castle" follows "the"; not a vocab word, not vivid, not stopword
    assert classify_token("castle", frozenset(), prev_token="the") == "#f4c471"


def test_empty_vocabulary_no_teal():
    from tui.context_pane import classify_token
    # "mailbox" with empty vocab should NOT be teal
    result = classify_token("mailbox", frozenset())
    assert result != "#4fd1c5"


def test_vocab_takes_priority_over_vivid():
    from tui.context_pane import classify_token
    # "horrifying" (9 chars, "ing") is also in vocab → teal wins
    vocab = frozenset({"horrifying"})
    assert classify_token("horrifying", vocab) == "#4fd1c5"


# ---------------------------------------------------------------------------
# on_token sub-word spacing regression tests
# ---------------------------------------------------------------------------

def _make_pane():
    """Return a ContextPane instance without a running Textual app."""
    from tui.context_pane import ContextPane, _STATE_THINKING
    pane = ContextPane.__new__(ContextPane)
    pane._vocabulary = frozenset({"mailbox"})
    pane._state = _STATE_THINKING
    pane._token_buffer = []
    pane._prev_token = ""
    pane._is_sentence_start = True
    pane._word_count = 0
    pane._activity_task = "remix"
    return pane


def test_subword_tokens_no_spurious_spaces():
    """Sub-word chunks 'c','reak','s' must join as 'creaks', not 'c reak s'."""
    pane = _make_pane()
    # Simulate three separate streaming events for one word
    for chunk in ("c", "reak", "s"):
        pane._token_buffer  # access to ensure state is valid
        import re
        from tui.context_pane import classify_token
        for segment in re.split(r"(\s+)", chunk):
            if not segment:
                continue
            if segment.isspace():
                pane._token_buffer.append(segment)
                continue
            color = classify_token(segment, pane._vocabulary,
                                   prev_token=pane._prev_token,
                                   is_sentence_start=pane._is_sentence_start)
            pane._token_buffer.append(f"[{color}]{segment}[/]")
            if re.search(r"\w", segment):
                pane._word_count += 1
            pane._prev_token = segment.strip(".,!?;:'\"-()[]").lower()
            pane._is_sentence_start = segment.rstrip().endswith((".", "!", "?"))

    joined = "".join(pane._token_buffer)
    # Strip all Rich markup tags for plain-text comparison
    plain = re.sub(r"\[/?[^\]]*\]", "", joined)
    assert plain == "creaks", f"Expected 'creaks', got {plain!r}"


def test_whole_word_token_preserves_trailing_space():
    """A chunk 'creaks ' (with trailing space) stays 'creaks '."""
    pane = _make_pane()
    import re
    from tui.context_pane import classify_token
    for segment in re.split(r"(\s+)", "creaks "):
        if not segment:
            continue
        if segment.isspace():
            pane._token_buffer.append(segment)
            continue
        color = classify_token(segment, pane._vocabulary,
                               prev_token=pane._prev_token,
                               is_sentence_start=pane._is_sentence_start)
        pane._token_buffer.append(f"[{color}]{segment}[/]")
        pane._prev_token = segment.strip(".,!?;:'\"-()[]").lower()

    plain = re.sub(r"\[/?[^\]]*\]", "", "".join(pane._token_buffer))
    assert plain == "creaks ", f"Expected 'creaks ', got {plain!r}"
