# Project Raijin-LOB

**Task-Aligned Latent Inference of Regime-Dependent Liquidity Manifolds**

Project Raijin-LOB aims to provide a high-fidelity latent representation of limit order book dynamics under structured interventional evaluations. Operating as a task-aligned latent inference engine, it hybridizes a bare-metal, cache-aligned C++ microstructure simulator driven by multivariate Hawkes processes and held-out families of strategically adaptive agent policies with a regime-conditioned Joint-Embedding Predictive Architecture.

To reduce entanglement between macro-state and transient microstructure, the model factorizes the latent space into regime and micro components, enforcing regime-invariance on the micro-state via Variance-Invariance-Covariance Regularization (VICReg) with controlled weighting, while encouraging separation through strict orthogonality constraints. Discontinuities are explicitly mapped via event-centric batching and contrastive boundary objectives defined around detected liquidity cliffs (e.g., thresholded spread expansions and queue depletion rates).

Rather than relying on direct autoregressive tick prediction, the architecture learns task-aligned latent transitions to support structured interventions, evaluating parameterized meta-orders in terms of impact response, fill probability, and adverse selection. To aggressively mitigate simulator-induced leakage, training and evaluation are strictly separated across out-of-distribution (OOD) agent policy classes and perturbed market regimes, with performance reported as statistically significant improvements over queue-reactive baselines.

Technical documentation: [docs site](https://lonelyguy-se1.github.io/Raijin-LOB/) (source in `docs/`).

Phase 1 Benchmark Results. 

benchmarks:
  - name: BM_BestBidAsk
    time_ns: 3.16
    cpu_ns: 3.16
    iterations: 219705540
    throughput_mps: 632.037

  - name: BM_Compare_Arka_AddNoMatch/manual_time
    time_ns: 41.8
    cpu_ns: 111
    iterations: 16735868
    throughput_mps: 23.9141

  - name: BM_Compare_Arka_CancelOnly/manual_time
    time_ns: 61.0
    cpu_ns: 111
    iterations: 11448388
    throughput_mps: 16.3969

  - name: BM_Compare_Arka_MatchOneLevel/manual_time
    time_ns: 54.6
    cpu_ns: 104
    iterations: 12859405
    throughput_mps: 18.3304

  - name: BM_Compare_Arka_MatchWithReceipts/manual_time
    time_ns: 57.0
    cpu_ns: 107
    iterations: 11338256
    throughput_mps: 17.5422

  - name: BM_Compare_NanoMatch_MixedAdd/manual_time
    time_ns: 66.3
    cpu_ns: 108
    iterations: 10382139
    throughput_mps: 15.0765

  - name: BM_MultiLevelSweep/manual_time
    time_ns: 106
    cpu_ns: 843
    iterations: 6590855
    throughput_mps: 9.42885

  - name: BM_MatchThroughTombstones/0/manual_time
    time_ns: 40.9
    cpu_ns: 806
    iterations: 17095797
    throughput_mps: 24.4236

  - name: BM_MatchThroughTombstones/8/manual_time
    time_ns: 62.1
    cpu_ns: 798
    iterations: 11282177
    throughput_mps: 16.0953

  - name: BM_MatchThroughTombstones/64/manual_time
    time_ns: 152
    cpu_ns: 888
    iterations: 4605999
    throughput_mps: 6.57272

  - name: BM_MatchThroughTombstones/256/manual_time
    time_ns: 497
    cpu_ns: 1241
    iterations: 1426432
    throughput_mps: 2.01325
