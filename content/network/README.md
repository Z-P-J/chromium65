content/network is a temporary location for the Network Service. While it will eventually end up in services/network, we can't move it now until content/common/url_loader.mojom can move. That is blocked on removing the old non-mojom IPC loading path (see LoadingWithMojo feature flag). Once that happens, the mojom can be self contained and we can move it and this code.