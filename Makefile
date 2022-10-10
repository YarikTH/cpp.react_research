##########################################################################
# configuration
##########################################################################

# find GNU sed to use `-i` parameter
SED:=$(shell command -v gsed || which sed)

##########################################################################
# documentation of the Makefile's targets
##########################################################################

# main target
all:
	@echo "This Makefile is for the maintenance of the repository, not for building."
	@echo "Supported targets:"
	@echo "* pretty_check - check if all cmake and c++ files are properly formatted"
	@echo "* pretty - prettify all cmake and c++ files"
	@echo "* amalgamated - generate ureact_amalgamated.hpp"


##########################################################################
# Prettify
##########################################################################
PRETTY_PY_COMMAND = /usr/bin/env python3 ./support/thirdparty/pretty.py/pretty.py
PRETTY_PY_OPTIONS = --clang-format clang-format-11
PRETTY_PY_OPTIONS += --exclude='support/thirdparty'

# check if all cmake and c++ files are properly formatted
pretty_check:
	@./support/venv.sh $(PRETTY_PY_COMMAND) $(PRETTY_PY_OPTIONS) --check -

# prettify all cmake and c++ files
pretty:
	@./support/venv.sh $(PRETTY_PY_COMMAND) $(PRETTY_PY_OPTIONS) -


##########################################################################
# Amalgamated
##########################################################################

# generate ureact_amalgamated.hpp
amalgamated:
	@./support/venv.sh /usr/bin/env python3 ./support/generate_amalgamated_file.py
