import numpy as np
import os
import math

def do_unmount_mount(mount_dev,mount_point):
    #import time
    #time.sleep(5)
    os.system("sudo umount {mount_point}".format(mount_point=mount_point))
    os.system("sudo mount {mount_dev} {mount_point}".format(mount_dev=mount_dev, mount_point=mount_point))


def string_inequality(str1, str2):
    print("str1 len: {str1_len}. str2 len: {str2_len}. #same char: {diff_char_num}.".format(str1_len=len(str1),str2_len=len(str2),diff_char_num=sum([str1[idx]==str2[idx] for idx in range(int(min(len(str1),len(str2))))])))
def do_write_read_test(file_name, file_name2, pos_list_beg, pos_list_end, pos_list2_beg, pos_list2_end, block_char_num, file_content, file_content2, mount_dev, mount_point, remount_prob, is_seek):
    if is_seek:
        fd_write_attribute="w"
    else:
        fd_write_attribute="a"
    for w_idx in range(len(pos_list_beg)):
        with open(file_name, fd_write_attribute) as fd:
            if is_seek:
                fd.seek(int(math.floor(pos_list_beg[w_idx] * block_char_num)))
            fd.write("".join(file_content[int(math.floor(pos_list_beg[w_idx] * block_char_num)):int(
                math.floor(pos_list_end[w_idx] * block_char_num))]))
        if np.random.uniform()<remount_prob:
            do_unmount_mount(mount_dev, mount_point)

        with open(file_name2, fd_write_attribute) as f2d:
            if is_seek:
                f2d.seek(int(math.floor(pos_list2_beg[w_idx] * block_char_num)))#This invokes library impl fseek(3) in OS
            f2d.write("".join(file_content2[
                              int(math.floor(pos_list2_beg[w_idx] * block_char_num)):int(
                                  math.floor(pos_list2_end[w_idx] * block_char_num))]))
        if np.random.uniform()<remount_prob:
            do_unmount_mount(mount_dev, mount_point)

        with open(file_name, "r") as fd:

            if is_seek:#equality test only the written part in this iteration
                fd.seek(int(math.floor(pos_list_beg[w_idx] * block_char_num)))
                file_curr_content=fd.read()[0:int(math.floor(pos_list_end[w_idx] * block_char_num))-int(math.floor(pos_list_beg[w_idx] * block_char_num))]
                print("test idx: " + str(w_idx) + " file 1 result: " + str("".join(file_content[int(math.floor(pos_list_beg[w_idx] * block_char_num)):int(math.floor(pos_list_end[w_idx] * block_char_num))]) == file_curr_content))
            else:#equaility test the whole file content
                file_curr_content = fd.read()
                print("test idx: " + str(w_idx) + " file 1 result: " + str(
                "".join(file_content[0:int(math.floor(pos_list_end[w_idx] * block_char_num))]) == file_curr_content))
                string_inequality("".join(file_content[0:int(math.floor(pos_list_end[w_idx] * block_char_num))]),file_curr_content)
        if np.random.uniform()<remount_prob:
            do_unmount_mount(mount_dev, mount_point)

        with open(file_name2, 'r') as f2d:
            if is_seek:#equality test only the written part in this iteration
                f2d.seek(int(math.floor(pos_list2_beg[w_idx] * block_char_num)))
                file_curr_content2 =f2d.read()[0:int(math.floor(pos_list2_end[w_idx] * block_char_num))-int(math.floor(pos_list2_beg[w_idx] * block_char_num))]
                print("test idx: " + str(w_idx) + " file 2 result: " + str(
                    "".join(file_content2[int(math.floor(pos_list2_beg[w_idx] * block_char_num)):int(math.floor(pos_list2_end[w_idx] * block_char_num))]) == file_curr_content2))
                string_inequality("".join(file_content2[int(math.floor(pos_list2_beg[w_idx] * block_char_num)):int(math.floor(pos_list2_end[w_idx] * block_char_num))]), file_curr_content2)
            else:#equaility test the whole file content
                file_curr_content2 = f2d.read()
                print("test idx: " + str(w_idx) + " file 2 result: " + str(
                    "".join(file_content2[0:int(math.floor(pos_list2_end[w_idx] * block_char_num))]) == file_curr_content2))
                string_inequality("".join(file_content2[0:int(math.floor(pos_list2_end[w_idx] * block_char_num))]), file_curr_content2)
        if np.random.uniform()<remount_prob:
            do_unmount_mount(mount_dev, mount_point)

def seek_write_test():
    block_size = 4  # in kb
    block_num = 7
    pos_list_beg = [1.2,4.1,2.4,4.7]
    pos_list_end = [2.4,4.7,4.1,7.0]
    pos_list2_beg = [1.2,4.1,2.4,4.7]
    pos_list2_end = [2.4,4.7,4.1,7.0]

    while True:
        this_run_random_int = int(np.random.randint(10000000))
        file_name = "./aufsMountPoint/test_multiple_block_seek_write_{}.log".format(this_run_random_int)
        file_name2 = "./aufsMountPoint/test_multiple_block_seek_write2_{}.log".format(this_run_random_int)
        if (not os.path.exists(file_name)) and (not os.path.exists(file_name2)):
            print(this_run_random_int)
            break
        else:
            print("conflict! retry this run random int\n")

    total_char_num = block_size * 1024 * block_num
    block_char_num = block_size * 1024
    file_content = np.random.randint(33, 127,
                                     size=total_char_num).tolist()  # devoid of special characters to avoid corner case
    file_content2 = np.random.randint(33, 127,
                                      size=total_char_num).tolist()  # devoid of special characters to avoid corner case

    file_content = list(map(lambda digit: chr(digit), file_content))
    file_content2 = list(map(lambda digit: chr(digit), file_content2))
    assert (len(pos_list_beg) == len(pos_list2_beg))
    assert (len(pos_list_end) == len(pos_list2_end))
    assert (len(pos_list_beg) == len(pos_list_end))
    do_write_read_test(file_name, file_name2, pos_list_beg, pos_list_end, pos_list2_beg, pos_list2_end, block_char_num,
                       file_content, file_content2, "/dev/nvme0n1", " /home/kwu/aufsMountPoint", 0, True)


def sequential_write_test():
    block_size = 4  # in kb
    block_num = 7
    pos_list = [0, 1.5, 3.3, 4.0, 4.7, 5.6, 7]
    pos_list2 = [0, 0.8, 1.0, 3.1, 4.5, 5.6, 7]

    while True:
        this_run_random_int = int(np.random.randint(10000000))
        file_name = "./aufsMountPoint/test_multiple_block_write_{}.log".format(this_run_random_int)
        file_name2 = "./aufsMountPoint/test_multiple_block_write2_{}.log".format(this_run_random_int)
        if (not os.path.exists(file_name)) and (not os.path.exists(file_name2)):
            print(this_run_random_int)
            break
        else:
            print("conflict! retry this run random int\n")

    total_char_num = block_size * 1024 * block_num
    block_char_num = block_size * 1024
    file_content = np.random.randint(33, 127,
                                     size=total_char_num).tolist()  # devoid of special characters to avoid corner case
    file_content2 = np.random.randint(33, 127,
                                      size=total_char_num).tolist()  # devoid of special characters to avoid corner case

    file_content = list(map(lambda digit: chr(digit), file_content))
    file_content2 = list(map(lambda digit: chr(digit), file_content2))
    assert (len(pos_list) == len(pos_list2))
    pos_list_beg = pos_list[0:-1]
    pos_list_end = pos_list[1:]
    pos_list2_beg = pos_list2[0:-1]
    pos_list2_end = pos_list2[1:]
    do_write_read_test(file_name, file_name2, pos_list_beg, pos_list_end, pos_list2_beg, pos_list2_end, block_char_num, file_content, file_content2, "/dev/nvme0n1", " /home/kwu/aufsMountPoint", 0, False)

if __name__ =="__main__":
    sequential_write_test()
    seek_write_test()
