build/MyApps:
	mkdir -p build
	docker run -e BUILD_TARGET=RELEASE -e DSC_PATH=MyApps/MyApps.dsc --rm -v $(PWD):/home/edk2/src -v $(PWD)/build:/home/edk2/Build xaionaro2/edk2-builder:vUDK2017
	find build/ -name *.efi

clean:
	rm -rf build
