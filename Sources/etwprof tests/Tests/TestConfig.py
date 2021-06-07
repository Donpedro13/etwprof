_testbin_folder_path = None

def set_testbin_folder_path(path : str):
    global _testbin_folder_path
    _testbin_folder_path = path

def get_process_timeout():
    return 10