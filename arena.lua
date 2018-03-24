-- puppet.lua
-- A simplified puppet without posable joints, but that
-- looks roughly humanoid.

rootnode = gr.node('root')

grey = gr.material({0.7, 0.7, 0.7}, {0.5, 0.5, 0.5}, 10)

arena = gr.mesh('icosphere_flip', 'arena')
rootnode:add_child(arena)
arena:set_material(grey)
arena:scale(50.0, 50.0, 50.0)

return rootnode
