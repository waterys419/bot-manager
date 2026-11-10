local frame=0
emu.addEventCallback(function()
 frame=frame+1
 if frame==120 then emu.log('BOOT 120 FRAMES'); emu.exit(0) end
end, emu.eventType.endFrame)
