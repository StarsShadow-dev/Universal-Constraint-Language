import { formatWithOptions } from "util";

type ReadFile = {
	path: string,
	byteSize: number,
}

let readFiles: ReadFile[] = [];
let totalReadFileByteSize = 0;

export default {
	readFile(data: ReadFile) {
		// console.log("readFile", data);
		readFiles.push(data);
		totalReadFileByteSize += data.byteSize;
	},
	
	printFileAccessLogs() {
		console.log("fileAccessLogs:");
		for (let i = 0; i < readFiles.length; i++) {
			console.log(`\t'${readFiles[i].path}': ${readFiles[i].byteSize}`);
		}
		console.log(`totalReadFileByteSize: ${totalReadFileByteSize}`);
	},
}