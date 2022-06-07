from distexprunner import ServerList, Server


SERVER_PORT = 3000

server_list = ServerList(
    Server('node01', '<ip>', SERVER_PORT, src_mac='<mac>'),
    Server('node02', '<ip>', SERVER_PORT, src_mac='<mac>'),
    Server('node03', '<ip>', SERVER_PORT, src_mac='<mac>'),
    Server('node04', '<ip>', SERVER_PORT, src_mac='<mac>'),
    Server('node05', '<ip>', SERVER_PORT, src_mac='<mac>'),
    Server('node06', '<ip>', SERVER_PORT, src_mac='<mac>'),
    Server('node07', '<ip>', SERVER_PORT, src_mac='<mac>'),
    Server('node08', '<ip>', SERVER_PORT, src_mac='<mac>'),

    Server('switch', '<ip>', SERVER_PORT),
    working_directory='/home/mjasny'
)
