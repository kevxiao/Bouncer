-- puppet.lua
-- A simplified puppet without posable joints, but that
-- looks roughly humanoid.

rootnode = gr.node('root')

green = gr.material({0.64, 1.0, 0.41}, {0.1, 0.1, 0.1}, 10)

goal = gr.mesh('sphere', 'goal')
rootnode:add_child(goal)
goal:set_material(green)
goal:scale(10.0, 10.0, 10.0)

return rootnode
