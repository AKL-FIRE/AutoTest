#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>

#include <cstdlib>
#include <cstdio>

#include <iostream>
#include <string>
#include <filesystem>
#include <regex>
#include <chrono>
#include <vector>

namespace fs = std::filesystem;

const int kMaxTime = 60;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

bool is_time_out = false;

long correct = 0;
long error = 0;

std::vector<std::string> errors;

void* timeout_handler(void* pid) {
  long id = (long)pid;
  sleep(kMaxTime);

  is_time_out = false;
  std::cout << "程序运行超时，杀死进程:" << id << std::endl;

  if(kill(id, SIGINT) == 0) {
    std::cout << "程序已被杀死" << std::endl;
  } else {
    std::perror("kill error");
  }
  is_time_out = true;
}

int main(int argc, const char* argv[]) {
  /*
   * argv[1]: program to be tested
   * argv[2]: input directory
   * argv[3]: output directory
   * */
  if (argc != 4) {
    std::perror("Usage:<program_name> <program_to_be_tested> <input_directory> <output_directory>");
    std::exit(-1);
  }

  // 1. 生成测试用临时文件夹
  fs::path test_temp_path("./temp");
  fs::directory_entry de(test_temp_path);
  if (de.exists()) {
    // delete that directory
    fs::remove_all(test_temp_path);
  }
  // create an empty directory
  fs::create_directory(test_temp_path);

  // 2. 读取input 和 output 文件夹的内容
  fs::path input_path(argv[2]);
  fs::path output_path(argv[3]);

  // 2.2 复制需要测试的程序到temp文件夹下
  fs::copy_file(argv[1], test_temp_path / "test");

  auto entry_iterator = fs::directory_iterator(input_path);
  std::cout << "自动化测试启动......" << std::endl;
  long total_tests = std::distance(entry_iterator, fs::directory_iterator());
  std::cout << "共有" << total_tests << "个测试目标" << std::endl;
  // 3. 开始进行测试
  long i = 0;
  for (auto& input_entry : fs::directory_iterator(input_path)) {
    std::cout << "当前进度" << "(" << ++i << "/" << total_tests << ")" << std::endl;
	std::cout << "--------------------------------------------" << std::endl;
    std::string filename = input_entry.path().filename();
    std::cout << "正在测试文件:" << filename << std::endl;
    // 提取出字符串中的数字
    std::regex re("[0-9]+");
    std::smatch match_result;
    std::regex_search(filename, match_result, re);
	std::string number = match_result[0].str();

	// 构建用于提取解文件的文件名
	fs::path solution_path = output_path;
	char temp[256];
	std::sprintf(temp, "cost%s.txt", number.c_str());
	std::string str_tmp(temp);
	solution_path += str_tmp;

    if (!solution_path.has_filename()) {
      std::cout << "No such file" << solution_path.filename() << " in directory: " << output_path << std::endl;
      std::exit(-1);
    }

    // 3.1 复制测试文件,结果到临时文件夹
    fs::path input_file_to = test_temp_path;
    input_file_to /= input_entry.path().filename();
    fs::path output_file_to = test_temp_path;
    output_file_to /= solution_path.filename();
    fs::copy_file(input_entry.path(), test_temp_path / "input.txt");
    fs::copy_file(solution_path, test_temp_path / "cost.txt");

    // 3.2 创建子进程用于运行program
    pid_t pid;
	// 记录程序开始时间
	auto start = std::chrono::steady_clock::now();
    if ((pid = fork()) == 0) {
      // 子进程
      // 更改工作路径
      if(chdir(test_temp_path.c_str()) != 0) {
        std::perror("Can't change the current working directory");
        std::exit(-1);
      }
      int res = execl("./test", NULL);
      if (res != 0) {
        std::perror("execl error");
        std::exit(-1);
      }
    } else {
      // 父进程
      // 开启一个子线程用于管理测试程序超时问题
      int status;
      pthread_t tid;
      pthread_create(&tid, NULL, timeout_handler, (void*)pid);
      waitpid(pid, &status, 0);
      pthread_cancel(tid);
      if (status != 0 && !is_time_out) {
        std::perror("run program error");
        std::exit(-1);
      }
      if (status == 0 && !is_time_out)
      	std::cout << "Program run successful" << std::endl;
    }
    auto end = std::chrono::steady_clock::now();
    std::cout << "测试程序共运行了："
    << std::chrono::duration_cast<std::chrono::seconds>(end - start).count()
    << "秒" << std::endl;

    // 3.3 比较两个文件是否相同
	int diff_r = std::system("diff -B -Z ./temp/cost.txt ./temp/output.txt");
	if (diff_r == 0) {
	  std::cout << "测试通过" << std::endl;
	  correct++;
	} else {
	  error++;
	  errors.push_back(filename);
	}
    std::cout << "清理本次残余文件。。。" << std::endl;
    fs::remove(test_temp_path / "input.txt");
    fs::remove(test_temp_path / "solution.txt");
    fs::remove(test_temp_path / "output.txt");
	fs::remove(test_temp_path / "cost.txt");
    std::cout << "--------------------------------------------" << std::endl << std::endl;
  }
  std::cout << "测试结束" << std::endl;
  std::cout << "测试结果(通过/全部):" << "(" << correct << "/" << total_tests << ")" << std::endl;
  std::cout << "正确率:" << (double)correct / (double)total_tests << std::endl;
  std::cout << "错误测试样例有: " << std::endl;
  std::cout << "[";
  for (const auto& e : errors) {
    std::cout << e << ", ";
  }
  std::cout << "]";
  std::cout << std::endl;
}
