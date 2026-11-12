-- M14: the arms the player sees in the first person (the skeleton of the first person with the hands on it, hanging from the camera,
-- holding the weapon that is out), in New Vegas. Out of the doctor's house into Goodsprings, the first ranged weapon with rounds in the
-- hand, the camera in the player's eyes: pictures of the hands as the weapon is drawn, held, raised and fired, a second weapon (a
-- two-handed one if the inventory has one), and the weapon put away. The engine's own lines say where the hands are ("Player arms");
-- the variants of rounds/m14-arms-tune.json move them with OPENFALLOUT_FIRSTPERSON_ARMS ("x y z yaw pitch roll" in the frame of the
-- camera: X right, Y ahead, Z up, degrees) to find the place the games have them at.
local NV = 'FalloutNV.esm'
return { limit = 300, delay = 4, run = function(r)
    local player = r.player()
    local function exterior() return player.cell ~= nil and player.cell.isExterior end
    r.god(true)
    r.activate({ NV, 0x103e61 }, 'door out')
    r.waitFor(exterior, 40, 'exterior')
    r.wait(10)
    r.settle(3, 30)
    r.equip(nil, 1)
    r.weapon('first')
    r.camera('FirstPerson')
    r.look(0, 0)
    r.wait(2)
    r.shot('A0_holstered')
    r.drawn(true)
    r.wait(0.3); r.shot('A1_draw_0.3s', false)
    r.wait(1.5); r.shot('A2_drawn', false)
    r.wait(2); r.shot('A2b_drawn_later', false)
    r.look(0, 0.25)
    r.wait(0.5); r.shot('A3_looking_down', false)
    r.look(0, -0.15)
    r.wait(0.5); r.shot('A3b_looking_up', false)
    r.look(0.6, 0)
    r.wait(0.5); r.shot('A3c_turned', false)
    r.look(0, 0)
    for i = 1, 3 do
        r.fire('arms_first_' .. i)
        r.wait(0.15); r.shot('A4_fire' .. i .. '_early', false)
        r.wait(0.8)
    end
    r.wait(1); r.shot('A5_after_fire', false)
    r.equip(nil, 2)
    r.weapon('second')
    r.wait(2); r.shot('A6_second_weapon', false)
    r.fire('arms_second_1')
    r.wait(0.15); r.shot('A7_second_fire', false)
    r.wait(1)
    r.drawn(false)
    r.wait(1.5); r.shot('A8_holstered_again', false)
    r.camera('ThirdPerson')
    r.wait(1.5); r.shot('B0_third_person')
    r.drawn(true)
    r.wait(2); r.shot('B1_third_person_drawn', false)
    r.camera('FirstPerson')
    r.wait(1.5); r.shot('A9_first_person_again', false)
end }
