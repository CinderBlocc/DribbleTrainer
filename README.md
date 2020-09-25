# DribbleTrainer

*This plugin works in freeplay only.*

DribbleTrainer contains training features for multiple aspects of dribbling:
- Dribble Mode: automatically resets the ball onto the player's car when it hits the ground.
- Flicks Mode: automatically resets the ball onto the player's car when they flick it a certain distance away. Logs the speed of the flick before resetting.
- Catch Training: launches the ball at the player's car from random angles.

Ball resetting works similar to the `ballontop` command that comes with BakkesMod, but this plugin takes the momentum of the car into account to calculate the ideal position of the ball when resetting.

For ease of use, click the "Dribble Trainer Binds" button in the quicksettings tab. This will set the DPad buttons (or 1-4 on keyboard) to the most useful binds from this plugin:
- [1 | DPad UP] - "DribbleReset" - Resets ball on car.
- [2 | DPad LEFT] - "DribbleRequestModeToggle dribble" - Toggle dribble mode.
- [3 | DPad DOWN] - "DribbleRequestModeToggle flick" - Toggle flicks mode.
- [4 | DPad RIGHT] - "DribbleLaunch" - Launch ball for a catch.
