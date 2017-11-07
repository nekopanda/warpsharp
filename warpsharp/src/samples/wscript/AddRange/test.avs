LoadPlugin("warpsharp.dll")
ws = WScript("AddRange.js", "JScript")

ColorBars(640, 480)
ShowFrameNumber()
ws.WSInvoke("AddRange", last, "test.vcf")
