s = '''
local_read_lock_failed=115
local_write_lock_failed=263
local_lock_success=14398807
local_lock_waiting=340
remote_lock_failed=33
remote_lock_success=1600181
remote_lock_waiting=48
'''


d = {x[0]: int(x[1])
     for x in map(lambda x: x.split('='), s.strip().split('\n'))}

local = d['local_read_lock_failed'] + \
    d['local_write_lock_failed'] + d['local_lock_success']
remote = d['remote_lock_failed'] + d['remote_lock_success']

print(local + remote)
print(f'remote_prob={remote/(local+remote):0.4f}')
