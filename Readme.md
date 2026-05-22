# Project Raijin-LOB

**Task-Aligned Latent Inference of Regime-Dependent Liquidity Manifolds**

Project Raijin-LOB aims to provide a high-fidelity latent representation of limit order book dynamics under structured interventional evaluations. Operating as a task-aligned latent inference engine, it hybridizes a bare-metal, cache-aligned C++ microstructure simulator driven by multivariate Hawkes processes and held-out families of strategically adaptive agent policies with a regime-conditioned Joint-Embedding Predictive Architecture.

To reduce entanglement between macro-state and transient microstructure, the model factorizes the latent space into regime and micro components, enforcing regime-invariance on the micro-state via Variance-Invariance-Covariance Regularization (VICReg) with controlled weighting, while encouraging separation through strict orthogonality constraints. Discontinuities are explicitly mapped via event-centric batching and contrastive boundary objectives defined around detected liquidity cliffs (e.g., thresholded spread expansions and queue depletion rates).

Rather than relying on direct autoregressive tick prediction, the architecture learns task-aligned latent transitions to support structured interventions, evaluating parameterized meta-orders in terms of impact response, fill probability, and adverse selection. To aggressively mitigate simulator-induced leakage, training and evaluation are strictly separated across out-of-distribution (OOD) agent policy classes and perturbed market regimes, with performance reported as statistically significant improvements over queue-reactive baselines.

Technical documentation: [docs site](https://lonelyguy-se1.github.io/Raijin-LOB/) (source in `docs/`).

Phase 1 Benchmark Results. 

-------------------------------------------------------------------------------------------------------------------
Benchmark                                                         Time             CPU   Iterations UserCounters...
-------------------------------------------------------------------------------------------------------------------
BM_BestBidAsk                                                  3.16 ns         3.16 ns    219705540 items_per_second=632.037M/s
BM_Compare_Arka_AddNoMatch/manual_time                         41.8 ns          111 ns     16735868 items_per_second=23.9141M/s
BM_Compare_Arka_CancelOnly/manual_time                         61.0 ns          111 ns     11448388 items_per_second=16.3969M/s
BM_Compare_Arka_MatchOneLevel/manual_time                      54.6 ns          104 ns     12859405 items_per_second=18.3304M/s
BM_Compare_Arka_MatchWithReceipts/manual_time                  57.0 ns          107 ns     11338256 items_per_second=17.5422M/s
BM_Compare_NanoMatch_MixedAdd/manual_time                      66.3 ns          108 ns     10382139 items_per_second=15.0765M/s
BM_MultiLevelSweep/manual_time                                  106 ns          843 ns      6590855 items_per_second=9.42885M/s
BM_MatchThroughTombstones/0/min_time:0.500/manual_time         40.9 ns          806 ns     17095797 items_per_second=24.4236M/s
BM_MatchThroughTombstones/8/min_time:0.500/manual_time         62.1 ns          798 ns     11282177 items_per_second=16.0953M/s
BM_MatchThroughTombstones/64/min_time:0.500/manual_time         152 ns          888 ns      4605999 items_per_second=6.57272M/s
BM_MatchThroughTombstones/256/min_time:0.500/manual_time        497 ns         1241 ns      1426432 items_per_second=2.01325M/s
