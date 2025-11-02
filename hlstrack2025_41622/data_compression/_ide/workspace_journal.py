# 2025-10-26T22:56:23.501466400
import vitis

client = vitis.create_client()
client.set_workspace(path="data_compression")

vitis.dispose()

