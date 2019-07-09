//! \file
//! \brief Configure and run our session of test units.

//! \def
//! \link https://github.com/catchorg/Catch2/blob/master/docs/own-main.md
#define CATCH_CONFIG_RUNNER
#include <catch.h>

//! \brief     Enhanced std::exit() when DEBUG mode is enabled.
//!            Test units are executed only when exit code is EXIT_SUCCESS.
//! \param[in] code Allows any exit code.
//! \note      Uncomment all 3 lines about --durations argument to
//!            visualize and understand the execution order of test units.
//! \note      While using --durations, the output should be read from top to bottom,
//!            but the output inside each scenario should be read from bottom to top.
void catch_exit(const int code) {
  // const char *argv[] = {"K", "--durations yes", nullptr};
  // const int argc = sizeof(argv) / sizeof(char*) - 1;
  exit(code ?: Catch::Session().run(
    // argc, argv
  ));
};
