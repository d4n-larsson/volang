# Designing a Language-Agnostic, Agent-Friendly Test Framework

## 1. Purpose and design principles

A traditional test framework is primarily designed for developers to
write, execute, and debug tests. A framework intended to work especially
well with AI agents should support the entire testing lifecycle:

1.  Understand what needs to be tested.
2.  Discover the framework's capabilities and existing tests.
3.  Generate and validate tests.
4.  Execute tests.
5.  Interpret results and diagnose failures.
6.  Improve and maintain tests.
7.  Verify that tests meaningfully cover the intended behavior.

The central design principle is:

> Make the framework easy to discover, easy to operate, predictable,
> composable, and capable of returning structured feedback to both
> humans and agents.

The framework should expose a language-independent interface.
Language-specific details should be handled by adapters rather than
leaking into the agent-facing API.

------------------------------------------------------------------------

## 2. Core framework capabilities

### 2.1 Test definition and creation

The framework needs a simple, expressive way to define tests.

  -----------------------------------------------------------------------
  Capability                          Description
  ----------------------------------- -----------------------------------
  Test definition                     Define individual test cases.

  Test suites                         Group tests into logical
                                      collections.

  Assertions                          Verify expected versus actual
                                      behavior.

  Setup and teardown                  Prepare and clean up test
                                      environments.

  Parameterized tests                 Execute the same test with
                                      different inputs.

  Fixtures                            Reusable test data, resources, and
                                      dependencies.

  Test dependencies                   Define prerequisites between tests
                                      when necessary.

  Tags and labels                     Categorize tests, such as smoke,
                                      regression, or integration.

  Metadata                            Attach descriptions, requirements,
                                      ownership, and other context.

  Skip and ignore                     Temporarily disable tests with an
                                      explicit reason.

  Expected failures                   Mark known failures without failing
                                      the entire suite.
  -----------------------------------------------------------------------

**Agent-oriented recommendation:** Tests should have explicit names,
descriptions, and metadata. An agent should be able to understand a
test's purpose without reverse-engineering its implementation.

Example test description:

``` json
{
  "test_name": "should_reject_invalid_user_email",
  "description": "Verifies that user registration rejects an invalid email address.",
  "input": {
    "email": "invalid-email"
  },
  "expected": {
    "status": 400,
    "error": "INVALID_EMAIL"
  }
}
```

The example is illustrative: the framework does not need to require
tests to be authored in JSON. It should, however, make their intent and
metadata available in a structured form.

### 2.2 Test discovery and exploration

Discovery is especially important for agents. An agent should not need
to read the entire framework source code to understand how to use it or
what already exists.

  -----------------------------------------------------------------------
  Capability                          Description
  ----------------------------------- -----------------------------------
  List tests                          Retrieve all available tests.

  Search tests                        Search by name, description, tag,
                                      or metadata.

  Describe test                       Retrieve detailed information about
                                      a test.

  List suites                         Discover available test suites.

  List assertions                     Discover supported assertion types.

  List fixtures                       Discover available fixtures.

  List plugins                        Discover available extensions.

  Inspect dependencies                Understand resources and components
                                      required by a test.

  Discover capabilities               Retrieve the framework's supported
                                      operations and features.
  -----------------------------------------------------------------------

Discovery operations should return structured data, preferably JSON,
with stable schemas.

Example:

``` json
{
  "test_name": "should_create_user",
  "description": "Verifies successful user creation",
  "tags": ["api", "integration"],
  "requirements": [
    "database",
    "test_user_fixture"
  ],
  "assertions": [
    "status_equals",
    "response_contains"
  ]
}
```

### 2.3 Test execution

The framework should provide a consistent execution interface.

  Capability             Description
  ---------------------- -----------------------------------------------------
  Run test               Execute an individual test.
  Run suite              Execute a complete suite.
  Run by tag             Execute tests matching selected tags.
  Run by pattern         Execute tests matching a name or pattern.
  Run all                Execute the entire test collection.
  Dry run                Validate tests without executing them.
  Parallel execution     Run independent tests simultaneously.
  Sequential execution   Execute tests in a defined order.
  Retry                  Retry failed tests according to an explicit policy.
  Timeout                Set execution time limits.
  Cancellation           Stop running tests.
  Execution filtering    Include or exclude tests based on criteria.

**Agent-oriented recommendation:** Make it straightforward to run one
test and receive a complete, machine-readable result. Do not rely solely
on terminal output or human-readable error messages.

Example:

``` json
{
  "test": "should_create_user",
  "status": "failed",
  "duration_ms": 120,
  "failure": {
    "type": "assertion_error",
    "expected": 201,
    "actual": 400,
    "location": "test_user.py:42"
  }
}
```

### 2.4 Assertions and verification

Assertions are the foundation of the framework. A useful assertion
library should cover common verification needs without forcing every
test to implement its own comparison logic.

**Basic assertions** - Equal and not equal - True and false - Null and
not null - Empty and not empty - Contains and does not contain - Greater
than and less than - Type and instance checks

**Collection assertions** - Contains an element - Contains all expected
elements - Has an exact size - Has unique elements - Matches a
structure - Matches a schema

**Exception assertions** - Throws an exception - Does not throw an
exception - Throws a specific exception type - Exception contains an
expected message

**Advanced assertions** - Snapshot comparison - Deep object comparison -
JSON schema validation - Regular-expression matching - Approximate
numerical comparison - State comparison - Time-based assertions

A particularly useful feature is **explainable assertions**.

Instead of returning only `Assertion failed`, return structured details:

``` json
{
  "assertion": "equals",
  "status": "failed",
  "expected": 201,
  "actual": 400,
  "explanation": "Expected HTTP status 201 but received 400."
}
```

This makes failures easier for an agent to interpret and act on.

### 2.5 Test data management

Agents frequently need to generate, reuse, and isolate test data.

The framework should support:

-   Test data generation
-   Randomized and deterministic data
-   Seed management
-   Data factories
-   Reusable test datasets
-   Data cleanup
-   Data isolation
-   Test data validation
-   Mock data generation

Reproducibility is essential. If an agent generates a failing test, it
should be possible to reproduce the same execution and data.

Example:

``` json
{
  "random_seed": 12345,
  "test_data": {
    "user_id": 123,
    "email": "test@example.com"
  }
}
```

### 2.6 Mocking and dependency management

Tests often depend on external systems. The framework should make it
possible to isolate those dependencies.

Potential capabilities:

-   Mock objects
-   Mock functions
-   Stub responses
-   Spy functionality
-   Dependency injection
-   Fake services
-   HTTP mocking
-   Database mocking
-   Event mocking
-   Time mocking
-   Environment-variable management

Where practical, support declarative configuration as well as
language-native mocking.

Example:

``` json
{
  "mock": {
    "service": "payment_service",
    "method": "charge",
    "response": {
      "status": "success"
    }
  }
}
```

### 2.7 Reporting and diagnostics

Reporting and diagnostics are among the most important areas for an
agent-oriented framework. An agent needs to understand not just whether
a test passed or failed, but why.

  -----------------------------------------------------------------------
  Capability                          Description
  ----------------------------------- -----------------------------------
  Test results                        Pass, fail, skipped, and other
                                      execution statuses.

  Execution logs                      Detailed execution information.

  Stack traces                        Error locations and call stacks.

  Failure explanations                Structured descriptions of
                                      failures.

  Execution duration                  Timing and performance information.

  Historical results                  Compare current and previous
                                      executions.

  Flaky test detection                Identify inconsistent test
                                      outcomes.

  Coverage reports                    Identify tested and untested code
                                      or behavior.

  Dependency failures                 Identify environmental or
                                      external-service problems.

  Debug information                   Provide relevant runtime state and
                                      diagnostic context.
  -----------------------------------------------------------------------

A useful additional concept is **failure classification**. Every failure
should have a machine-readable category, such as:

``` text
ASSERTION_FAILURE
TEST_SETUP_FAILURE
TEST_DATA_FAILURE
ENVIRONMENT_FAILURE
DEPENDENCY_FAILURE
TIMEOUT
FLAKY_TEST
TEST_IMPLEMENTATION_ERROR
```

The category helps an agent choose an appropriate response:

-   Assertion failure: investigate the observed application behavior.
-   Test implementation error: inspect and potentially fix the test.
-   Environment failure: investigate the test environment.
-   Dependency failure: investigate an external service or resource.
-   Timeout: investigate execution limits, performance, or deadlocks.

The framework should distinguish a failure in the system under test from
a failure in the test infrastructure wherever possible.

### 2.8 Test modification and maintenance

An agent-oriented framework should support maintaining tests, not just
generating them.

  Capability             Description
  ---------------------- ---------------------------------------------
  Update test            Modify an existing test.
  Delete test            Remove a test.
  Rename test            Change its identifier or name.
  Clone test             Create a copy of an existing test.
  Validate test          Check test structure and configuration.
  Compare tests          Identify differences between test versions.
  Detect duplicates      Identify overlapping or redundant tests.
  Analyze test quality   Identify weak or incomplete tests.
  Suggest tests          Suggest missing test scenarios.

A particularly valuable capability is **test quality evaluation**. The
framework should help identify:

-   Tests without meaningful assertions.
-   Tests that always pass or do not exercise the intended behavior.
-   Tests that duplicate existing coverage.
-   Tests that depend on execution order.
-   Tests with non-deterministic behavior.
-   Tests that are too tightly coupled to implementation details.

The goal is not for the framework to declare tests correct with
certainty. It is to provide evidence and warnings that help an agent
assess their quality.

### 2.9 Coverage and impact analysis

The framework should help agents understand what is not being tested and
what may be affected by a code change.

Potential capabilities:

-   Code coverage
-   Branch coverage
-   Function coverage
-   Requirement coverage
-   API endpoint coverage
-   Test-to-requirement mapping
-   Change impact analysis
-   Missing-test detection

Example:

``` json
{
  "component": "UserService",
  "coverage": {
    "functions": 85,
    "branches": 65
  },
  "untested_functions": [
    "delete_user",
    "reset_password"
  ]
}
```

An agent can use this information to propose additional tests.

An important distinction: high code coverage does not necessarily mean
good behavioral coverage. The framework should ideally support both
code-oriented and requirement- or behavior-oriented coverage.

------------------------------------------------------------------------

## 3. Agent-specific interface

The framework should expose a predictable, composable interface designed
for programmatic use. This could be provided through an API,
command-line interface, tool interface, or a combination of these.

A proposed set of agent-facing operations:

  Function                    Purpose
  --------------------------- --------------------------------------------
  `discover()`                Discover framework capabilities.
  `list_tests()`              List existing tests.
  `describe_test()`           Retrieve details about a particular test.
  `list_assertions()`         Discover available assertions.
  `create_test()`             Create a test.
  `update_test()`             Modify a test.
  `validate_test()`           Validate test structure and configuration.
  `run_test()`                Execute an individual test.
  `run_suite()`               Execute a test suite.
  `get_result()`              Retrieve execution results.
  `get_logs()`                Retrieve execution logs.
  `get_failure_details()`     Retrieve structured failure information.
  `get_coverage()`            Retrieve coverage information.
  `analyze_failure()`         Classify and analyze a failure.
  `suggest_tests()`           Suggest missing test scenarios.
  `evaluate_test_quality()`   Evaluate generated tests.

These operations should have:

1.  Predictable input schemas.
2.  Predictable output schemas.
3.  Clear error handling.
4.  Explicit side-effect behavior.
5.  Consistent naming conventions.
6.  Discoverable documentation.
7.  Idempotent behavior where possible.
8.  Stable identifiers for tests and execution runs.

For example, `run_test()` should return the same conceptual result
structure regardless of whether the underlying test is implemented in
Python, JavaScript, Java, or another language.

The agent interface should also distinguish between operations that
inspect information and operations that change state or execute code.
This makes permissions, approvals, and auditing easier to implement.

------------------------------------------------------------------------

## 4. Recommended architecture

A useful architecture separates the agent-facing interface from the core
framework and language-specific implementations.

``` text
                     AI AGENT
                         |
                         |
                 AGENT INTERFACE
                         |
          +--------------+--------------+
          |              |              |
      Discovery       Creation       Execution
          |              |              |
          +--------------+--------------+
                         |
                  TEST FRAMEWORK
                         |
          +--------------+--------------+
          |              |              |
      Assertions      Fixtures       Reporting
          |              |              |
          +--------------+--------------+
                         |
                 LANGUAGE ADAPTERS
                         |
          +--------------+--------------+
          |              |              |
        Python       JavaScript        Java
          |              |              |
          +--------------+--------------+
                         |
                    APPLICATION
```

The key architectural decision is that the agent interface should be
independent of the programming language. Language-specific behavior
should be implemented behind adapters.

This allows an agent to use the same conceptual operations regardless of
the language in which a test is written.

------------------------------------------------------------------------

## 5. Recommended MVP

It would be risky to implement every capability at once. Start by
validating the most important agent workflows.

A practical initial scope is:

  ---------------------------------------------------------------------------
  Priority                Function                    Reason
  ----------------------- --------------------------- -----------------------
  P0                      `discover()`                The agent can
                                                      understand the
                                                      framework.

  P0                      `list_tests()`              The agent can discover
                                                      existing tests.

  P0                      `describe_test()`           The agent can
                                                      understand existing
                                                      tests.

  P0                      `create_test()`             The agent can generate
                                                      tests.

  P0                      `validate_test()`           The agent can check
                                                      test structure before
                                                      execution.

  P0                      `run_test()`                The agent can execute
                                                      an individual test.

  P0                      `run_suite()`               The agent can execute
                                                      multiple tests.

  P0                      `get_result()`              The agent receives
                                                      structured results.

  P0                      `get_failure_details()`     The agent can
                                                      understand failures.

  P0                      `get_logs()`                The agent can
                                                      investigate execution
                                                      problems.

  P1                      `update_test()`             The agent can maintain
                                                      tests.

  P1                      `get_coverage()`            The agent can identify
                                                      missing coverage.

  P1                      `suggest_tests()`           The agent can identify
                                                      missing scenarios.

  P1                      `evaluate_test_quality()`   The agent can assess
                                                      generated tests.

  P1                      `analyze_failure()`         The agent can get
                                                      structured failure
                                                      analysis.
  ---------------------------------------------------------------------------

The P0 capabilities enable a basic end-to-end workflow: discover,
create, validate, execute, and inspect. P1 capabilities improve the
agent's ability to maintain and improve the suite.

------------------------------------------------------------------------

## 6. Two possible product directions

There are two different products that could be built.

### Option A: Agent-compatible test framework

Build a conventional test framework with additional APIs that make it
easier for agents to interact with.

This is primarily a developer productivity tool. It focuses on making
existing testing workflows accessible and reliable for agents.

### Option B: Agent-native testing platform

Build a system where an agent can:

1.  Explore an application.
2.  Understand its requirements and behavior.
3.  Identify missing test scenarios.
4.  Generate tests.
5.  Execute them.
6.  Analyze failures.
7.  Improve the tests.
8.  Maintain the test suite over time.

This is closer to an autonomous testing system. It requires more than a
test runner: it also needs application context, requirement
understanding, test planning, and mechanisms for evaluating whether
generated tests are meaningful.

These directions are related, but they are not the same product.

A sensible strategy is to design the underlying framework so it can
support Option B, while initially delivering the smaller and more
achievable scope of Option A.

------------------------------------------------------------------------

## 7. Additional design principles for agent usability

Beyond the feature list, several cross-cutting properties will strongly
influence how easy the framework is for agents to use.

### Structured, stable contracts

Use explicit schemas for inputs, outputs, errors, and execution results.
Version those schemas and avoid changing them unexpectedly.

### Discoverability

The agent should be able to ask the framework what operations,
assertions, fixtures, adapters, and configuration options are available.
Documentation should be accessible in a machine-readable form as well as
human-readable form.

### Actionable errors

Errors should identify what failed, where it failed, why it failed when
known, and what information is available for further diagnosis.

### Reproducibility

Record relevant execution context, including random seeds,
configuration, versions, and test data references. Make it possible to
rerun a specific execution.

### Isolation and safe execution

Tests should be isolated where possible. Destructive actions, external
calls, resource usage, and access to secrets should be controlled
explicitly. Agents should not receive broader permissions than they
need.

### Observability

Expose execution status, logs, diagnostics, and progress in ways that
can be retrieved programmatically. Long-running executions should have
stable run IDs and support status checks and cancellation.

### Composability

Keep operations small and focused. An agent should be able to combine
discovery, validation, execution, and diagnostics into a workflow
without needing one enormous operation that hides what it is doing.

### Human review and auditability

Make generated or modified tests easy to inspect. Record what an agent
changed and why, and support review or approval before consequential
actions when appropriate.

------------------------------------------------------------------------

## 8. Final recommendation

Do not optimize only for how easily an agent can write a test. Optimize
for how easily it can complete the entire testing lifecycle:

**Discover → Understand → Create → Validate → Execute → Diagnose →
Improve → Maintain**

That lifecycle is the real design target.

The most important foundation is a language-independent, structured
agent interface with reliable discovery, test creation, validation,
execution, and diagnostics. Once that foundation exists, richer
capabilities such as test quality evaluation, coverage analysis, and
autonomous test maintenance can be added without tying the framework to
one programming language.
