-- puppet.lua
-- A simplified puppet without posable joints, but that
-- looks roughly humanoid.

rootnode = gr.node('root')

lblue = gr.material({0.67, 0.74, 1.0}, {0.1, 0.1, 0.1}, 10)

player = gr.mesh('sphere', 'player')
rootnode:add_child(player)
player:set_material(lblue)
player:scale(1.0, 1.0, 1.0)

return rootnode
