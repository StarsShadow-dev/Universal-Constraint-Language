type ReadFile = {
	path: string,
	byteSize: number,
}

let readFiles: ReadFile[] = [];
let totalReadFileByteSize = 0;

const times: any = {};

export default {
	readFile(data: ReadFile) {
		// console.log("readFile", data);
		readFiles.push(data);
		totalReadFileByteSize += data.byteSize;
	},
	
	addTime(name: string, time: number) {
		if (!times[name]) times[name] = 0;
		times[name] += time;
	},
	
	//#region printing
	
	printFileAccessLogs() {
		console.log("fileAccessLogs:");
		for (let i = 0; i < readFiles.length; i++) {
			console.log(`\t'${readFiles[i].path}': ${readFiles[i].byteSize}`);
		}
		console.log(`totalReadFileByteSize: ${totalReadFileByteSize}`);
	},
	
	printTimes() {
		console.log("times:");
		for (const key in times) {
			console.log(`\t${key}: ${times[key]}ms`);
		}
	},
	
	//#endregion printing
}