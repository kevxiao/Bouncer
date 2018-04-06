# Bouncer

You play as the blue ball, trying to hit the red ball to get it into the green goal.

See a video demo here: 
[![Bouncer](http://img.youtube.com/vi/iEwRRAtY8vY/0.jpg)](http://www.youtube.com/watch?v=iEwRRAtY8vY)

## Compilation:
    premake4 gmake
    make

## Run:
Run with `./Bouncer sphere` for a sphere arena or `./Bouncer cube` for a cube arena

`./Bouncer` defaults to a sphere arena

## Controls:
Use the mouse to control camera rotation  
Press / hold WASD for movement up, left, down, right and SPACE to move forward  
Hold shift to speed boost (move twice as fast)  
Press Z to dance / play an animation  

Press P or Escape to pause / resume  
Press R to reset game  
Press M to show framerate  

Press/hold C to cheat (send red ball towards goal)  

## Features:
Modelling the scene - Arena with player object and ball  
Application UI - Title screen, pause screen, win screen and framerate display  
Texture mapping - Concrete texture mapped to arena walls  
Particle system - Particles explosion on ball collision and goal explosions on win screen  
Keyframe based animations using linear interpolation - Six small ball animations around player on pressing Z  
Sound - Bouncing sound, boost sound, win sound, win explosion sound, boost sound, background music  
Static collision detection - Ball and player colliding with arena walls, ball colliding with stationary goal on cube arena  
Dynamic collision detection - Ball colliding with moving player, ball colliding with moving goal on sphere arena  
Motion blur using post-processing depth buffer - Blurred scene and player when using speed boost  
Shadows using depth maps - Object in scene casting shadows on each other from two point light sources  