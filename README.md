# FAQ

## What is Allie?

Allie is a new and original chess engine heavily inspired by the seminal AlphaZero paper and the [Lc0](https://lczero.org "Lc0") project.

## How is she related to Leela?

Like Leela, Allie is based off of the same concepts and algorithms that were introduced by Deepmind in the AlphaZero paper(s), but her code is original and contains an alternative implementation of those ideas. You can think of Allie as a young cousin of Leela that utilizes the same networks produced by the Lc0 project.

## Is this like Anti-fish or Leelenstein?

Not exactly. AF and the Stein are (apparently quite successful) experiments with the training procedures that go into making a Lc0 project neural net. However, they both utilize the Lc0 binary as the actual chess engine and just load these alternative networks. Allie is different. She can be used as a complete replacement of the Lc0 binary with different search, hash, move generation, etc, etc. That means that Allie can be used with the AF or Stein networks too.

## Ok, so details. How is she different?

Well, I was inspired during the original CCC to see if you could pair traditional AlphaBeta search with an NN. This is still her main purpose and the focus going forward. However, the initial pre-release version is using much the same MCTS algorithm as Lc0 with a few differences. Here is a non-exhaustive list of some of the other differences:
- The threading model is different
- She does not have fpu-reduction
- Optimized magic bitboard move generation
- Her time mgmt. is completely custom
- She will show mate distance for her eval at the end of the game if she finds it

## All right, brass tacks how strong is she?

Depends. First, her ELO will obviously depend upon which network is used. However, head-to-head using the same network on my limited hardware setup she is 50-100 elo behind.

## Why did you develop her rather than just help out Leela?

A couple reasons. First, my original inspiration was to see if I could implement an alternative search using AlphaBeta rather than MCTS. Second, I wanted to teach myself the AlphaZero concepts and algorithms and this was the best way to do it. In the future, I expect Allie to have an AB search and leave MCTS to Leela, but it is not ready yet. I've done a lot of experiments with AB and Lc0 networks and think I've hit upon what might work, but getting the NPS to scale and utilizing the policy values optimally is challenging. Stay tuned.

## Ok, so she uses Lc0's networks. Why don't you make your own?

Two reasons. First, I couldn't hope to compete with the machine learning and AI experts who are contributing to the Lc0 project. Second, it would take a lot of compute power to train a new network and I just don't have that.

## Supporting further development of Allie

I've set up a patreon page here: https://www.patreon.com/gonzochess75 and would greatly appreciate any support from the community to be used to test Allie on more multi-gpu systems.

## Anything else?

Yes, I'd like to wholeheartedly thank the developers of the Lc0 project which make Allie even possible. Only by standing on their shoulders could Allie exist. I'd also like to thank Deepmind for their work on AlphaZero which started the whole NN for chess era. Finally, I'd like to thank Andrew Grant who wrote the Ethereal engine whose code was a big inspiration for writing Allie and to CCC and TCEC for including the engine in their tournaments.

