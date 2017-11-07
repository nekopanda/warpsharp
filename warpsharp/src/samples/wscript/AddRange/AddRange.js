var File = new ActiveXObject("Scripting.FileSystemObject")
var RE = /^VirtualDub\.subset\.Add(Range|Frame)\((\d+),(\d+)\);$/

function AddRange(clip, filename) {
	var fp = File.OpenTextFile(filename, 1)
	var st = ""

	AVS.last = clip

	while(!fp.AtEndOfStream) {
		var r = RE.exec(fp.ReadLine())

		if(r == null)
			continue

		if(!st.empty)
			st += "+"

		st += "Trim(" + r[2] + "," + r[2] + "+" + r[3] + "-1" + ")"
	}

	fp.close()

	return st.empty ? clip : AVS.Eval(st)
}
