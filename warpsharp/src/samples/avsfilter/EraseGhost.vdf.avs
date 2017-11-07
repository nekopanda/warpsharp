info = LoadPlugin("warpsharp.dll")
filt = AvisynthFilters("EraseGhost", info)

func = filt.AddFunction("ConvertToYUY2", hidden = true)

func = filt.AddFunction("EraseGhost")

func.AddTrack("位置", 0, -256, 256, -256.0, 256.0)
func.AddTrack("強さ", 0, -256, 256, -256.0, 256.0)

func.AddTrack("位置", 0, -256, 256, -256.0, 256.0)
func.AddTrack("強さ", 0, -256, 256, -256.0, 256.0)

func.AddTrack("位置", 0, -256, 256, -256.0, 256.0)
func.AddTrack("強さ", 0, -256, 256, -256.0, 256.0)

func.AddTrack("位置", 0, -256, 256, -256.0, 256.0)
func.AddTrack("強さ", 0, -256, 256, -256.0, 256.0)

func.AddTrack("位置", 0, -256, 256, -256.0, 256.0)
func.AddTrack("強さ", 0, -256, 256, -256.0, 256.0)

func.AddTrack("位置", 0, -256, 256, -256.0, 256.0)
func.AddTrack("強さ", 0, -256, 256, -256.0, 256.0)

func.AddTrack("位置", 0, -256, 256, -256.0, 256.0)
func.AddTrack("強さ", 0, -256, 256, -256.0, 256.0)

func.AddTrack("位置", 0, -256, 256, -256.0, 256.0)
func.AddTrack("強さ", 0, -256, 256, -256.0, 256.0)

func.AddTrack("位置", 0, -256, 256, -256.0, 256.0)
func.AddTrack("強さ", 0, -256, 256, -256.0, 256.0)

func.AddTrack("位置", 0, -256, 256, -256.0, 256.0)
func.AddTrack("強さ", 0, -256, 256, -256.0, 256.0)

func.AddTrack("位置", 0, -256, 256, -256.0, 256.0)
func.AddTrack("強さ", 0, -256, 256, -256.0, 256.0)

func.AddTrack("位置", 0, -256, 256, -256.0, 256.0)
func.AddTrack("強さ", 0, -256, 256, -256.0, 256.0)

func.AddTrack("位置", 0, -256, 256, -256.0, 256.0)
func.AddTrack("強さ", 0, -256, 256, -256.0, 256.0)

func.AddTrack("位置", 0, -256, 256, -256.0, 256.0)
func.AddTrack("強さ", 0, -256, 256, -256.0, 256.0)

func.AddTrack("位置", 0, -256, 256, -256.0, 256.0)
func.AddTrack("強さ", 0, -256, 256, -256.0, 256.0)

func = filt.AddFunction("ConvertToRGB32", hidden = true)

return filt
