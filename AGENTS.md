## Adding SloeJIT Instructions

When adding SloeJIT AArch64 instruction support, use Arm's official
machine-readable A64 ISA XML specs as the source of truth where possible.

Prefer finding the latest A64 ISA XML bundle from Arm's A-profile architecture
or exploration tools download page. A known direct URL at the time of writing
is:

`https://developer.arm.com/-/cdn-downloads/permalink/Exploration-Tools-A64-ISA/ISA_A64/ISA_A64_xml_A_profile-2026-03_96.tar.gz`

This URL is release-tagged and may go stale. If the direct URL fails and a
current URL cannot be found, stop and ask the user for the updated Arm A64 ISA
XML tarball URL, or ask them to provide the current tarball or extracted XML. Do
not continue by reconstructing instruction encodings from memory.

After implementing instructions, add tests and rely on
`sloejit/test/verify_binary_test_cases.py` to verify expected binary opcodes
against assembler/objdump output. The agent should review that each test
assembly string matches the corresponding SloeJIT IR builder call, then ask the
user to do the same before considering the change complete.
