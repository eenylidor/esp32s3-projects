# Navigate to you CLionProjects directory.
cd ~/CLionProjects/
# Clone the repository without all the files.
git clone --filter=blob:none --sparse https://github.com/eenylidor/esp32s3-projects.git
# Navigate inside the repo online to the directory of the project you want to work on.
cd esp32s3-projects/
# request the files for the project you want to work on.
git sparse-checkout set esp32s3_projects/<project_name>