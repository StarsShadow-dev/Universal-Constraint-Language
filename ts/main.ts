import { compileFile } from "./compiler";

try {
	compileFile("test/main.llcl");	
} catch (error) {
	if (error == "__do nothing__") {
		// do nothing
	} else {
		throw error;
	}
}