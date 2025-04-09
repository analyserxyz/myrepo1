This's a bridge between SAS language and llama.cpp, allowing SAS programs to call llama.cpp(https://github.com/ggml-org/llama.cpp) to run large models locally.

Install:
1. Download the appropriate llama.cpp binary package from https://github.com/ggml-org/llama.cpp/releases according to your OS and hardware configuration.
2. Unzip the llama.cpp binary package and copy all .dll files and llama-cli.exe to C:\Program Files\SASHome\SASFoundation\9.4\core\sasexe.
3. Download and unzip https://github.com/user-attachments/files/19668932/ollama.zip to C:\Program Files\SASHome\SASFoundation\9.4\core\sasexe.
4. Now the C:\Program Files\SASHome\SASFoundation\9.4\core\sasexe contains ollama.dll and all dll files from llama.cpp binary package, for example:
  ggml-base.dll      
  ggml-cpu.dll       
  ggml-rpc.dll       
  ggml.dll           
  libcurl-x64.dll    
  llama.dll
  llava_shared.dll   
  ollama.dll
5. Download a gguf format model, for example: [https://huggingface.co/ggml-org/gemma-1.1-7b-it-Q4_K_M-GGUF/tree/main](https://huggingface.co/ggml-org/gemma-1.1-7b-it-Q4_K_M-GGUF/blob/main/gemma-1.1-7b-it.Q4_K_M.gguf)
6. The procedure syntax:
   proc ollama [out=libref] [debug] [timeout=n];
   model "";
   params ""; //detailed parameter: https://github.com/ggml-org/llama.cpp/blob/master/examples/main/README.md
   run;
   
Suppose model gemma-1.1-7b-it.Q4_K_M.gguf is in c:\temp, then SAS code to run this model:

//library to store the model result

libname tmplib "c:\temp";

proc ollama out=tmplib.result;

model "c:\temp\gemma-1.1-7b-it.Q4_K_M.gguf";

params "-no-cnv -ngl 99 -p ""Once upon a time""";

run;

The output:
![image](https://github.com/user-attachments/assets/2ecba401-6529-44c8-b240-de82f94c4ef6)




