<div align="center">

<pre>
██████╗  █████╗ ██╗     ██╗██╗███╗   ██╗
██╔══██╗██╔══██╗██╗     ██║██║████╗  ██║
██████╔╝███████║██╗     ██║██║██╔██╗ ██║
██╔══██╗██╔══██║██╗██   ██║██║██║╚██╗██║
██║  ██║██║  ██║██╗╚█████╔╝██║██║ ╚████║
╚═╝  ╚═╝╚═╝  ╚═╝╚═╝ ╚════╝ ╚═╝╚═╝  ╚═══╝
</pre>

[![License](https://img.shields.io/badge/License-GPLv3-22c55e?style=for-the-badge)](https://github.com/LonelyGuy-SE1/Raijin-LOB/blob/main/LICENSE)
[![Docs](https://img.shields.io/badge/Docs-Reference-4a90d9?style=for-the-badge&logo=githubpages&logoColor=white)](https://lonelyguy-se1.github.io/Raijin-LOB/)
[![CI](https://img.shields.io/badge/CI-Passing-2088ff?style=for-the-badge&logo=githubactions&logoColor=white)](https://github.com/LonelyGuy-SE1/Raijin-LOB/actions)
![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-GCC%20%7C%20Clang-fcc624?style=for-the-badge&logo=linux&logoColor=black)

**Production-grade limit order book core feeding a JEPA world model for market microstructure.**

</div>

---

Project Raijin-LOB aims to provide a high-fidelity latent representation of limit order book dynamics under structured interventional evaluations. Operating as a task-aligned latent inference engine, it hybridizes a bare-metal, cache-aligned C++ microstructure simulator driven by multivariate Hawkes processes and held-out families of strategically adaptive agent policies with a regime-conditioned Joint-Embedding Predictive Architecture.

To reduce entanglement between macro-state and transient microstructure, the model factorizes the latent space into regime and micro components, enforcing regime-invariance on the micro-state via Variance-Invariance-Covariance Regularization (VICReg) with controlled weighting, while encouraging separation through strict orthogonality constraints. Discontinuities are explicitly mapped via event-centric batching and contrastive boundary objectives defined around detected liquidity cliffs (e.g., thresholded spread expansions and queue depletion rates).

Rather than relying on direct autoregressive tick prediction, the architecture learns task-aligned latent transitions to support structured interventions, evaluating parameterized meta-orders in terms of impact response, fill probability, and adverse selection. To aggressively mitigate simulator-induced leakage, training and evaluation are strictly separated across out-of-distribution (OOD) agent policy classes and perturbed market regimes, with performance reported as statistically significant improvements over queue-reactive baselines.

## Phase 1 Benchmark Results

Linux CI, AMD EPYC 9V74, median of 3 runs, Release build with LTO.

| Benchmark                         | Time (ns) | Throughput (M/s) |
| --------------------------------- | --------- | ---------------- |
| BM_BestBidAsk                     | 0.35      | 5,660            |
| BM_Compare_Arka_AddNoMatch        | 41.3      | 24.2             |
| BM_Compare_Arka_CancelOnly        | 33.8      | 29.6             |
| BM_Compare_Arka_MatchOneLevel     | 37.4      | 26.7             |
| BM_Compare_Arka_MatchWithReceipts | 38.0      | 26.3             |
| BM_Compare_NanoMatch_MixedAdd     | 52.2      | 19.1             |
| BM_MultiLevelSweep (5-level)      | 82.5      | 12.1             |
| BM_MatchThroughTombstones/0       | 37.8      | 26.5             |
| BM_MatchThroughTombstones/64      | 112       | 8.9              |
| BM_MatchThroughTombstones/256     | 345       | 2.9              |
| BM_RandomAdd                      | 61.6      | 16.2             |
| BM_RandomCancel                   | 48.3      | 20.7             |
| BM_RandomMatch                    | 62.2      | 16.1             |
| BM_ReplaySynthetic (1M msgs)      | -         | 28.6M msgs/sec   |

Latency histograms (CPU cycles):

| Benchmark               | p50 | p90 | p99 |
| ----------------------- | --- | --- | --- |
| AddNoMatch (random IDs) | 130 | 208 | 468 |
| Cancel (random IDs)     | 78  | 208 | 520 |
| MatchOneLevel           | 78  | 78  | 78  |
| MatchWithReceipts       | 78  | 78  | 104 |
| TombstoneMatch/256      | 884 | 910 | 910 |

## License

Licensed under the [GNU General Public License v3.0](https://github.com/LonelyGuy-SE1/Raijin-LOB/blob/main/LICENSE).
