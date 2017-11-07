info = LoadPlugin("warpsharp.dll")
filt = AvisynthFilters("WarpSharp", info)

func = filt.AddFunction("ConvertToYUY2", hidden = true)

func = filt.AddFunction("WarpSharp")
func.AddTrack("ã≠Ç≥", 128, 0, 1024)
func.AddTrack("ùÚÇµ",   3, 1,    9)
func.AddTrack("Ëáíl", 128, 0, 1024)

func = filt.AddFunction("ConvertToRGB32", hidden = true)

return filt
