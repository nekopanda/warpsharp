import os, re, sys, getopt

_pattern = re.compile('^\s*#\s*include\s*([<"])(.+)[">]')
_inclist = []
_depends = {}

def search_lines(file):
	incs = []

	for line in file.xreadlines():
		res = _pattern.search(line)
		if res:
			incs.append(list(res.groups()))

	return incs


def search_files(filename, basepath = '.', bracket = '"'):
	if bracket != '<':
		dirs = [basepath or '.'] + _inclist
	else:
		dirs = list(_inclist)

	for path in dirs:
		normpath = os.path.normpath(path + os.sep + filename)

		if _depends.has_key(normpath):
			return normpath

		if os.path.exists(normpath):
			break
	else:
		return None

	incs = search_lines(open(normpath))
	path = os.path.split(normpath)[0]

	_depends[normpath] = incs

	for inc in incs:
		inc.append(search_files(inc[1], path, inc[0]))

	return normpath


def main():
	opts, args = getopt.getopt(sys.argv[1:], 'I:')

	for opt in opts:
		if opt[0] == '-I':
			_inclist.append(opt[1])

	for arg in args:
		search_files(arg)

	keys = _depends.keys()
	keys.sort()

	for key in keys:
		print key + ':',

		for inc in _depends[key]:
			if inc[2]:
				print inc[2],

		print


if __name__ == '__main__':
	main()
