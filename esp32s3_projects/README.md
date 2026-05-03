# *📚 Mono-repo Workflow: Single Project Operations*





#### *Part 1: Pull a Single Project (Sparse Checkout)*

###### Navigate to you CLionProjects directory.



Bash:       cd \~/CLionProjects/

Powershell: cd \~\\CLionProjects\\



###### Clone the repository without all its files.

git clone --filter=blob:none --sparse https://github.com/eenylidor/esp32s3-projects.git



###### Navigate inside the repo online to the directory of the project you want to work on.

cd esp32s3-projects/



###### request the files for the project you want to work on.

git sparse-checkout set esp32s3\_projects/<project\_name>







#### *Part 2: Push a Single New Project (Sparse Add)*

###### Clone the repository structure (Sparse Clone)



git clone --filter=blob:none --sparse https://github.com/eenylidor/esp32s3-projects.git

cd esp32s3-projects/



Bash:       cp -r \~/Path/To/my\_new\_project/ esp32s3\_projects/

Powershell: Copy-Item -Path "C:\\Path\\To\\my\_new\_project" -Destination "esp32s3\_projects\\" -Recurse



git add esp32s3\_projects/<project\_name>/



git commit -m "Added new project: <project\_name>"

git push

