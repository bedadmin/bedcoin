Bedcoin Core integration/staging tree
=====================================

https://bedcoin.cc

What is BedCoin?
----------------

We propose Bit Ecological Digital Asset ("BED"), which uses a hybrid
consensus algorithm of Proof of Work (PoW) and Proof of Stake (PoS) to ensure the
security and fairness of the blockchain network, At the same time, anonymity
technology development and off-chain transaction system development have been
added to protect user transaction privacy and meet large-scale commercial
micropayment needs.

For more information, as well as an immediately usable, binary version of
the Bitcoin Core software, see https://www.bedcoin.cc/download.html, or read the
[original whitepaper](https://www.bedcoin.cc/dow/BED.pdf).

License
-------

Bedcoin Core is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/licenses/MIT.

Development Process
-------------------

The `master` branch is regularly built and tested, but is not guaranteed to be
completely stable. [Tags](https://github.com/bedadmin/bedcoin/tags) are created
regularly to indicate new official, stable release versions of Bedcoin Core.

The contribution workflow is described in [CONTRIBUTING.md](CONTRIBUTING.md)
and useful hints for developers can be found in [doc/developer-notes.md](doc/developer-notes.md).

Testing
-------

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test on short notice. Please be patient and help out by testing
other people's pull requests, and remember this is a security-critical project where any mistake might cost people
lots of money.

### Automated Testing

Developers are strongly encouraged to write [unit tests](src/test/README.md) for new code, and to
submit new unit tests for old code. Unit tests can be compiled and run
(assuming they weren't disabled in configure) with: `make check`. Further details on running
and extending unit tests can be found in [/src/test/README.md](/src/test/README.md).

There are also [regression and integration tests](/test), written
in Python, that are run automatically on the build server.
These tests can be run (if the [test dependencies](/test) are installed) with: `test/functional/test_runner.py`

The Travis CI system makes sure that every pull request is built for Windows, Linux, and macOS, and that unit/sanity tests are run automatically.

### Manual Quality Assurance (QA) Testing

Changes should be tested by somebody other than the developer who wrote the
code. This is especially important for large or high-risk changes. It is useful
to add a test plan to the pull request description if testing the changes is
not straightforward.

