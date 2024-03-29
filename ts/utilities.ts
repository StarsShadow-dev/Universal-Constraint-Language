import { exit } from "process";

export default {
	unreachable(): never {
		debugger;
		console.error("unreachable code reached");
		exit(1);
	}
}