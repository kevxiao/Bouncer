-- puppet.lua
-- A simplified puppet without posable joints, but that
-- looks roughly humanoid.

rootnode = gr.node('root')

lblue = gr.material({0.67, 0.74, 1.0}, {0.1, 0.1, 0.1}, 10)
lpurp = gr.material({0.76, 0.55, 1.0}, {0.1, 0.1, 0.1}, 10)

player = gr.mesh('sphere', 'player')
rootnode:add_child(player)
player:set_material(lblue)

ext1 = gr.mesh('sphere', 'ext1')
player:add_child(ext1)
ext1:set_material(lpurp)
ext1:scale(0.2, 0.2, 0.2)
ext1:translate(0.85, 0.0, 0.0)

ext2 = gr.mesh('sphere', 'ext2')
player:add_child(ext2)
ext2:set_material(lpurp)
ext2:scale(0.2, 0.2, 0.2)
ext2:translate(-0.85, 0.0, 0.0)

ext3 = gr.mesh('sphere', 'ext3')
player:add_child(ext3)
ext3:set_material(lpurp)
ext3:scale(0.2, 0.2, 0.2)
ext3:translate(0.0, 0.85, 0.0)

ext4 = gr.mesh('sphere', 'ext4')
player:add_child(ext4)
ext4:set_material(lpurp)
ext4:scale(0.2, 0.2, 0.2)
ext4:translate(0.0, -0.85, 0.0)

ext5 = gr.mesh('sphere', 'ext5')
player:add_child(ext5)
ext5:set_material(lpurp)
ext5:scale(0.2, 0.2, 0.2)
ext5:translate(0.0, 0.0, 0.85)

ext6 = gr.mesh('sphere', 'ext6')
player:add_child(ext6)
ext6:set_material(lpurp)
ext6:scale(0.2, 0.2, 0.2)
ext6:translate(0.0, 0.0, -0.85)

return rootnode
