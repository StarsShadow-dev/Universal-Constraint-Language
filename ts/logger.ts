type ReadFile = {
	path: string,
	byteSize: number,
}
type WriteFile = {
	path: string,
	byteSize: number,
}

let readFiles: ReadFile[] = [];
let totalReadFileByteSize = 0;

let writeFiles: ReadFile[] = [];
let totalWriteFileByteSize = 0;

const times: any = {};

const globals: any[][] = [];

export enum LogType {
	DB,
	build,
	evaluate,
}

export default {
	readFile(data: ReadFile) {
		// console.log("readFile", data);
		readFiles.push(data);
		totalReadFileByteSize += data.byteSize;
	},
	
	writeFile(data: WriteFile) {
		// console.log("writeFile", data);
		writeFiles.push(data);
		totalWriteFileByteSize += data.byteSize;
	},
	
	addTime(name: string, time: number) {
		if (!times[name]) times[name] = 0;
		times[name] += time;
	},
	
	log(type: LogType, ...args: any[]) {
		if (LogType[type] != "DB") return;
		console.log(`[${LogType[type]}]`, ...args);
		// globals.push(args);
	},
	
	//#region printing
	
	printFileAccessLogs() {
		console.log(`totalReadFileByteSize: ${totalReadFileByteSize}`);
		console.log(`totalWriteFileByteSize: ${totalWriteFileByteSize}`);
		console.log("readFiles:");
		for (let i = 0; i < readFiles.length; i++) {
			console.log(`\t'${readFiles[i].path}': ${readFiles[i].byteSize}`);
		}
		console.log("writeFiles:");
		for (let i = 0; i < writeFiles.length; i++) {
			console.log(`\t'${writeFiles[i].path}': ${writeFiles[i].byteSize}`);
		}
	},
	
	printTimes() {
		console.log("times:");
		for (const key in times) {
			console.log(`\t${key}: ${times[key]}ms`);
		}
	},
	
	//#endregion printing
}