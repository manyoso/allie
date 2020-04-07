# FAQ

## What is Allie?

Allie is a new and original chess engine heavily inspired by the seminal AlphaZero paper and the [Lc0](https://lczero.org "Lc0") project.

## How is she related to Leela?

Like Leela, Allie is based off of the same concepts and algorithms that were introduced by Deepmind in the AlphaZero paper(s), but her code is original and contains an alternative implementation of those ideas. You can think of Allie as a young cousin of Leela that utilizes the same networks produced by the Lc0 project.

## Is this like Anti-fish or Leelenstein?

Not exactly. AF and the Stein are (apparently quite successful) experiments with the training procedures that go into making a Lc0 project neural net. However, they both utilize the Lc0 binary as the actual chess engine and just load these alternative networks. Allie is different. She can be used as a complete replacement of the Lc0 binary with different search, hash, move generation, etc, etc. That means that Allie can be used with the AF or Stein networks too.

## Ok, so details. How is she different?

Well, I was inspired during the original CCC to see if you could pair traditional Minimax/AlphaBeta search with an NN. This is still her main purpose and the focus going forward. However, the initial versions were using a similar pure MCTS algorithm as Lc0 and AlphaZero. The current versions of Allie use a modified hybrid search of Minimax and Monte Carlo.

Here is a non-exhaustive list of differences:
- UCI protocol code
- Input/Output
- Time managment
- Board representation
- Move generation
- Zobrist keys
- Hash implementation
- The threading model
- Search algorithm
- Tree structure
- The multi-gpu scaling code
- Fpu-reduction
- Mate distance eval
- Testing framework and tests
- Debugging code

## What bits are used from the Lc0 project?

Here is what Allie uses from the Lc0 codebase:

- Protocol buffers for an NN weights file
- Code for discovering/loading the NN weights file
- Backend code for GPU to get evaluations given an NN weights file

## All right, brass tacks how strong is she?

Depends. First, her ELO will obviously depend upon which network is used. However, head-to-head using the same network on my limited hardware setup she is very competitive with Lc0 and SF.

## Why did you develop her rather than just help out Leela?

A couple reasons. First, my original inspiration was to see if I could implement an alternative search using Minimax/AlphaBeta rather than MCTS. Second, I wanted to teach myself the AlphaZero concepts and algorithms and this was the best way to do it. Allie is now using a hybrid Minimax Monte Carlo search.

Also, I am contributing back some patches to Leela where appropriate.

## Ok, so she uses Lc0's networks. Why don't you make your own?

Two reasons. First, I couldn't hope to compete with the machine learning and AI experts who are contributing to the Lc0 project. Second, it would take a lot of compute power to train a new network and I just don't have that.

## Supporting further development of Allie

I've set up a patreon page here: https://www.patreon.com/gonzochess75 and would greatly appreciate any support from the community to be used to test Allie on more multi-gpu systems.

## Anything else?

Yes, I'd like to wholeheartedly thank the developers of the Lc0 project which make Allie even possible. Only by standing on their shoulders could Allie exist. I'd also like to thank Deepmind for their work on AlphaZero which started the whole NN for chess era. Finally, I'd like to thank Andrew Grant who wrote the Ethereal engine whose code was a big inspiration for writing Allie and to CCC and TCEC for including the engine in their tournaments.

# Quick start guide (Linux)

## Prerequisites

You need:

- working build environment: compiler (e.g., GCC or CLANG), binutils, and the rest of the usual stuff.
- qmake (part of Qt SDK)
- CUDA and cuDNN. If you can build and run Lc0 with the cuDNN backend, you're probably good.

## Building

The basic steps:

- `qmake` --- Generates the makefiles. Usually only needed to run once.
- `make -j` --- Build Allie.

If everything went well, you now have `bin/allie` .

If the build system cannot locate CUDA or you want to use a specific CUDA version, you can use NVCC, CUDA_INC_DIR and CUDA_LIB_DIR. For instance:

  `make -j CUDA_INC_DIR=/opt/cuda-10.1.168/include CUDA_LIB_DIR=/opt/cuda-10.1.168/lib64 NVCC=/opt/cuda-10.1.168/bin/nvcc`

Similarly, you can specify the C++ compiler:

  `make -j CXX=clang++-7`

To clean up all the build temporaries:

  `make clean`

There's also `make distclean` to clean up everything, including the targets and the generated makefiles. You need to re-run `qmake` after this command. If in doubt whether changing compiler/CUDA/whatever is in effect, use `make distclean`.

## Running Allie

- Add the network weights file in the same directory with the Allie binary. Symlink (`ln -s`) can be used
- Launch `allie`. If everything went well, you'll see "allie" in stylished ASCII art with the version information
- You should now be ready to use Allie with your favorite UCI-compatible chess GUI.
