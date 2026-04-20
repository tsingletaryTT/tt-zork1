# SPDX-License-Identifier: Apache-2.0
# Smoke test: confirms TT-Lang pyenv is working for the Zork project.
import sys
sys.path.insert(0, str(__import__("pathlib").Path(__file__).parent))

import torch
import ttnn
from sim import ttl
from sim.dfb import Block
from sim.ttnnsim import Tensor

def main():
    data = torch.tensor([ord(c) for c in "ZORK I"], dtype=torch.float32)
    print("TT-Lang pyenv OK. Data:", data.tolist())
    print("First byte:", int(data[0]), "=", chr(int(data[0])))

if __name__ == "__main__":
    main()
