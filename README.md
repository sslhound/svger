docker build -t svger:dev --progress=plain .
docker run -v d:/Projects/sslhound/svger/results:/app/results/ svger:dev