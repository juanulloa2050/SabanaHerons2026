# Sabana-Herons Code Release

This is the 2024 Sabana-herons code release. this code is heavily based in B-Human 2023 code release [available in](https://wiki.b-human.de/coderelease2023/) as we are a small team mainly composed by undergrad students.

Nonetheless, we have added our own improvements to the code, and behaviors that adapt the best to our strategy, therefore, the following list is meant to describe those improvements and adaptations we've done:

## Improvements:

1. **5vs5 tactics and strategies**: Due to our number of available NaoV6 robots and team size, we're currently participating exclusively in SPL Shield Challenge, we've adapted and created B-Human code to work in 5vs5 matches.
2. **Fail analysis with back head button**: Due to the difficulty to rapidly and consistently detect fails in our current field setup (a kind of true official game field but - which due to installation can generate some problems in robots motion schema), we've developed a functionality that when can output the most **"Critical recent fails"** so we don't have to access each robot's log.
3. **Improved DribbleToGoal Skill**: We noted DribbleToGoal skill may encounter some edge cases for Striker, especially when arriving to the delimitation lines of field, which caused the striker to lose the ball and generate a Goal Kick In for adversaries or to shot at the post of the goal, we tried to improve that by almost never letting the striker shot the ball outside field boundary generating a KickIn or striking the Goal Post.

## Run this code

As we said, this code is heavily based in B-Human 2023 code release [available in](https://wiki.b-human.de/coderelease2023/), therefore and so far, you can just follow their documentation to build and run the code.
