-- puppet.lua
-- A simplified puppet without posable joints, but that
-- looks roughly humanoid.

rootnode = gr.node('root')

white = gr.material({1.0, 1.0, 1.0}, {0.1, 0.1, 0.1}, 10)
red = gr.material({1.0, 0.0, 0.0}, {0.1, 0.1, 0.1}, 10)

ball = gr.mesh('sphere', 'ball')
rootnode:add_child(ball)
ball:set_material(red)
ball:scale(0.5, 0.5, 0.5)

return rootnode
