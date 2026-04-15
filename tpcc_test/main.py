import os
import subprocess
import time
import csv


NUM_TESTS = 4
TESTS = ["load_test",
         "check_load_test",
         "first_tpcc",
         "only_read_tpcc"]

def get_test_name(test_case):
    return "/test_sql/"+str(test_case)+".sql"



# def get_house():
#     git_url = "https://gitlab.eduxiji.net/202310616992005/11.git"
#     branch_name = "load_7"
#     project_name = git_url.split('/')[-1].replace('.git', '')
#     if os.path.exists(project_name):  # 判断项目目录是否存在
#         subprocess.run(["rm", "-rf", project_name])  # 如果存在，使用 rm -rf 命令删除
#     subprocess.run(["git", "clone", "-b", branch_name, git_url])
#     os.chdir(project_name)

def build():
    # change dir to root
    if os.path.exists("./build"):
        os.system("rm -rf build")
    os.mkdir("./build")
    os.chdir("./build")
    os.system("cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE=-O2 ..")
    os.system("make rmdb -j4")
    os.chdir("..")
    os.chdir("../test_cpp")
    if os.path.exists("./build"):
        os.system("rm -rf build")
    os.mkdir("./build")
    os.chdir("./build")
    os.system("cmake ..")
    os.system("make")
    os.chdir("../../11")
    
def count_lines_csv(file_path):
    with open(file_path, 'r') as file:
        csv_reader = csv.reader(file)
        line_count = sum(1 for _ in csv_reader)
    return line_count

def run():
    os.chdir("./build")
    test_cpp_file = "../../test_cpp/build/bin/"
    table_basic_path = "../../performance_test/table_data"
    warehouse_num = 50
    max_thread_num = 16
    # 检测是否需要生成数据
    check_path = table_basic_path+"/warehouse.csv"
    cout_num = count_lines_csv(check_path)
    print(f"current table warehouse num is {cout_num}")
    if cout_num<=warehouse_num:
        print("generate csv data")
        os.chdir("../../generate_data")
        os.system("make")
        os.system("./main")
        os.chdir("../11/build")
    cout_num_now = count_lines_csv(check_path)
    print(f"current table warehouse num is {cout_num_now}")

    # 记录开始时间
    start_time = time.time()
    database_name = "tpcc_test"
    os.system("./bin/rmdb " + database_name + " &")
    end_time = time.time()
    running_time = (end_time - start_time) * 1000000
    print(f"open db use: {running_time} 微秒")
    time.sleep(7)
    # 测试load 语句
    load_start_time = time.time()
    print("[-----------Transaction Testing load_data case ...-----------]")
    os.system(test_cpp_file+"basic_send "+ "../../test_sql/load.sql")
    load_end_time = time.time()
    load_running_time = (end_time - start_time) * 1000000
    print(f"[-----------You have pass Transaction Testing load_data case-----------]")
   
    time.sleep(5)
    print("[-----------Transaction Testing check_load_data case ...-----------]")
    check_load_data_start_time = time.time()
    os.system(test_cpp_file+"basic_send "+ "../../test_sql/check_load.sql")
    check_load_data_end_time = time.time()
    check_load_data_running_time = (check_load_data_end_time - check_load_data_end_time) * 1000000
    print(f"[-----------You have pass Transaction Testing check_load_data case-----------]")
    time.sleep(5)
    # 生成tpcc的sql文件
    # os.system(test_cpp_file+"generate_sql "+str(warehouse_num))
    print("[-----------Transaction Testing tpcc_first_phase case ...-----------]")
    # 运行 tpcc测试
    os.system(test_cpp_file+"tpcc_first_phase "+ str(max_thread_num)+" 16100")
    print("[-----------You have Pass Transaction Testing tpcc_first_phase case ...-----------]")

    os.system("ps -ef | grep rmdb | grep -v grep | awk '{print $2}' | xargs kill -9")
    print("finish kill")
       


if __name__ == "__main__":
    get_house()
    build()
    run()
    
