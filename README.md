This is a conservative fork of angrylion's RDP plugin that aims to add new features while retaining the accuracy of the original plugin.

Current features:
* More maintainable code base by dividing the huge n64video.cpp into smaller pieces.
* Improved portability by separating the emulator plugin interface and window management from the RDP emulation core.
* Multi-threaded rendering support, which increases performance on multi-core CPUs significantly.
