/* SabanaHerons fork extension (B-Human 2023 base).
 * Execute bridge-originated requests through the existing skill dispatcher so Python does
 * not bypass B-Human motion control.
 * Release overview and commit references: README.md.
 */

option(HandleRLRequest)
{
  initial_state(start)
  {
    transition
    {
      goto execute;
    }
  }

  state(execute)
  {
    action
    {
      executeRequest();
    }
  }
}
