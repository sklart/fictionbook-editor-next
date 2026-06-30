ImportEPUB stabilization notes for large EPUB test sets
=======================================================

Recommended target scenario:
  3178 EPUB files, about 4.28 GB total input size.

Recommended workflow:
  1. BUILD_STABLE_TEST_RELEASE.cmd
  2. Edit BIG_VOLUME_TEST.cmd:
       INPUT_DIR
       OUTPUT_DIR
       PACKAGE_DIR
       RESUME_MODE
  3. BIG_VOLUME_TEST.cmd

Important large-test defaults:
  --flush-report-each
    CSV/HTML reports are written after every processed EPUB. If the run is
    interrupted, the report still contains the already processed files.

  --quiet
    Reduces console output for thousands of files.

  RESUME_MODE=1
    Adds --skip-existing. Re-running BIG_VOLUME_TEST.cmd skips FB2 files that
    already exist in OUTPUT_DIR.

  MAX_FILES=0
    Processes all files. Set MAX_FILES=50 for a smoke run before the full test.

Package policy:
  BIG_VOLUME_TEST.cmd does not package all source EPUB and all FB2 files by
  default. It creates a compact ZIP with reports, console log, binaries, and up
  to MAX_PROBLEM_ROWS problematic EPUB/FB2/log pairs.

Recommended first smoke run:
  Set MAX_FILES=50 and run BIG_VOLUME_TEST.cmd.
  If OK, set MAX_FILES=0 and run the full collection.

Files produced:
  OUTPUT_DIR\ImportEPUBBatch_report.csv
  OUTPUT_DIR\ImportEPUBBatch_report.html
  OUTPUT_DIR\ImportEPUB_big_test_console_YYYYMMDD_HHMMSS.log
  OUTPUT_DIR\ImportEPUB_big_test_summary_YYYYMMDD_HHMMSS.txt
  PACKAGE_DIR\ImportEPUB_big_test_YYYYMMDD_HHMMSS.zip
