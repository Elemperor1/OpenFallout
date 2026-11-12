local NV = 'FalloutNV.esm'
return { limit = 200, delay = 4, run = function(r)
    local player = r.player()
    r.activate({ NV, 0x103e61 }, 'door out')
    r.waitFor(function() return player.cell ~= nil and player.cell.isExterior end, 40, 'exterior')
    r.wait(12)
    r.eye('door')
    local offsets = {}
    for dx = -2000, 8000, 500 do for dy = -3000, 6000, 500 do table.insert(offsets, { dx, dy }) end end
    r.ground('grid', offsets)
end }
