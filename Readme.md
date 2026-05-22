# Project Raijin-LOB

**Task-Aligned Latent Inference of Regime-Dependent Liquidity Manifolds**

Project Raijin-LOB aims to provide a high-fidelity latent representation of limit order book dynamics under structured interventional evaluations. Operating as a task-aligned latent inference engine, it hybridizes a bare-metal, cache-aligned C++ microstructure simulator driven by multivariate Hawkes processes and held-out families of strategically adaptive agent policies with a regime-conditioned Joint-Embedding Predictive Architecture.

To reduce entanglement between macro-state and transient microstructure, the model factorizes the latent space into regime and micro components, enforcing regime-invariance on the micro-state via Variance-Invariance-Covariance Regularization (VICReg) with controlled weighting, while encouraging separation through strict orthogonality constraints. Discontinuities are explicitly mapped via event-centric batching and contrastive boundary objectives defined around detected liquidity cliffs (e.g., thresholded spread expansions and queue depletion rates).

Rather than relying on direct autoregressive tick prediction, the architecture learns task-aligned latent transitions to support structured interventions, evaluating parameterized meta-orders in terms of impact response, fill probability, and adverse selection. To aggressively mitigate simulator-induced leakage, training and evaluation are strictly separated across out-of-distribution (OOD) agent policy classes and perturbed market regimes, with performance reported as statistically significant improvements over queue-reactive baselines.

Technical documentation: [docs site](https://lonelyguy-se1.github.io/Raijin-LOB/) (source in `docs/`).

Phase 1 Benchmark Results. 

| Benchmark                                    | Time (ns) | CPU (ns) | Iterations | Throughput (M/s) |
|---------------------------------------------|-----------|-----------|-------------|------------------|
| BM_BestBidAsk                               | 3.16      | 3.16      | 219705540   | 632.037          |
| BM_Compare_Arka_AddNoMatch/manual_time      | 41.8      | 111       | 16735868    | 23.9141          |
| BM_Compare_Arka_CancelOnly/manual_time      | 61.0      | 111       | 11448388    | 16.3969          |
| BM_Compare_Arka_MatchOneLevel/manual_time   | 54.6      | 104       | 12859405    | 18.3304          |
| BM_Compare_Arka_MatchWithReceipts/manual_time | 57.0    | 107       | 11338256    | 17.5422          |
| BM_Compare_NanoMatch_MixedAdd/manual_time   | 66.3      | 108       | 10382139    | 15.0765          |
| BM_MultiLevelSweep/manual_time              | 106       | 843       | 6590855     | 9.42885          |
| BM_MatchThroughTombstones/0/manual_time     | 40.9      | 806       | 17095797    | 24.4236          |
| BM_MatchThroughTombstones/8/manual_time     | 62.1      | 798       | 11282177    | 16.0953          |
| BM_MatchThroughTombstones/64/manual_time    | 152       | 888       | 4605999     | 6.57272          |
| BM_MatchThroughTombstones/256/manual_time   | 497       | 1241      | 1426432     | 2.01325          |
