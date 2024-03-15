/**
 * @file PlaySoccer.h
 *
 * This file defines the root option of the soccer behavior.88899
 *
 * @author Thomas Röfer
 */

option(PlaySoccer)
{
  initial_state(selectOption)
  {
    action
    {
      select_option(options); //@options
    }
  }
}
