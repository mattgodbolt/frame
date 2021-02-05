.DEFAULT_GOAL := help

help:
	@grep -E '^[0-9a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":"}; {split($$3,a,"\#\#"); printf "\033[36m%-30s\033[0m %s (%s)\n", $$2, a[2], $$1}'

build:

    
