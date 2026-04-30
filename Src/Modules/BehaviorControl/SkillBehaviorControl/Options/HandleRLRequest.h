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
