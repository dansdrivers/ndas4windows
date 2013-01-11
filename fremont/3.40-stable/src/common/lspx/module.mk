# This makefile include is for xplat build.
nxp-lspx-path=lspx
nxp-lspx-src=$(addprefix $(nxp-lspx-path)/,lsp.c lsp_hash.c lsp_util.c lsp_debug.c)
nxp-lspx-obj=$(nxp-lspx-src:%.c=$(nxp-build)/%.o)

