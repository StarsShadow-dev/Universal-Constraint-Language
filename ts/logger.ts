type readFile = {
	path: string,
	byteSize: number,
}

let readFiles: readFile[] = [];

export default {
	readFile(data: readFile) {
		// console.log("readFile", data);
		readFiles.push(data);
	},
	
	printFileAccessLogs() {
		for (let i = 0; i < readFiles.length; i++) {
			console.log(readFiles[i]);
		}
	},
}