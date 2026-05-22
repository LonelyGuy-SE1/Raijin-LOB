# Project Raijin-LOB

**Task-Aligned Latent Inference of Regime-Dependent Liquidity Manifolds**

Project Raijin-LOB aims to provide a high-fidelity latent representation of limit order book dynamics under structured interventional evaluations. Operating as a task-aligned latent inference engine, it hybridizes a bare-metal, cache-aligned C++ microstructure simulator driven by multivariate Hawkes processes and held-out families of strategically adaptive agent policies with a regime-conditioned Joint-Embedding Predictive Architecture.

To reduce entanglement between macro-state and transient microstructure, the model factorizes the latent space into regime and micro components, enforcing regime-invariance on the micro-state via Variance-Invariance-Covariance Regularization (VICReg) with controlled weighting, while encouraging separation through strict orthogonality constraints. Discontinuities are explicitly mapped via event-centric batching and contrastive boundary objectives defined around detected liquidity cliffs (e.g., thresholded spread expansions and queue depletion rates).

Rather than relying on direct autoregressive tick prediction, the architecture learns task-aligned latent transitions to support structured interventions, evaluating parameterized meta-orders in terms of impact response, fill probability, and adverse selection. To aggressively mitigate simulator-induced leakage, training and evaluation are strictly separated across out-of-distribution (OOD) agent policy classes and perturbed market regimes, with performance reported as statistically significant improvements over queue-reactive baselines.

Technical documentation: [docs site](https://lonelyguy-se1.github.io/Raijin-LOB/) (source in `docs/`).
