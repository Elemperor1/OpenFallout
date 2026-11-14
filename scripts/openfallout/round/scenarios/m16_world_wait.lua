-- M16 world: does nothing but wait, so that another script (the autoplay tool) plays the game and a round only supplies the
-- hidden engine, the isolated folders and the log.
return { limit = 700, delay = 4, run = function(r)
    r.wait(660)
end }
