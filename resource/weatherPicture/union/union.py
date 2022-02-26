union_file_name = "union.txt"
with open(union_file_name,'w') as file:
     for i in range(40):
          subfile_name = r"D:\workspace\eletric\esp_c\main\素材\天气图像\batch\%d.c"%int(i)
          print(i)
          with open(subfile_name,'r') as subfile:
               text = subfile.readlines()
               text[-1] = text[-1].replace(';',',')
               text[0] = "{\n"
               for each in text:
                    file.write(each)
