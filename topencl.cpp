#include <stdio.h>
#include <stdlib.h>
#include <CL/cl.h>
#include <sys/time.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

const char* clutl_GetErrorString(int errorCode);
void clutl_device_caps(cl_device_id device);
int clutl_platform_select(int select_id,		cl_platform_id * ptr_platform_id);
int clutl_build_program(cl_context context, 	cl_device_id device,	const char * filename,	cl_program *ptr_program);
void run_myGEMM(cl_platform_id platform_id, const char * kernel_filename);
#define clutl_CheckError(errorCode) \
    if (errorCode != 0) {\
        fprintf(stderr, ">>> **** %s:%d  %s\n",__FILE__,__LINE__, clutl_GetErrorString(errorCode));\
    }


void matmult(float *A, float *B, float * C, int M, int K, int N);
int matcmp(float *A, float *B, int M, int N);
double gettime_sec(void);
int load_bin(const char *filename, void * data, int size);
int save_bin(const char *filename, void * data, int size);


int main(int argc, char * argv[])
{
	int cnt, i;
	cl_platform_id platform_id = NULL;
    //setenv("OCL_OUTPUT_ASM","1",1);
    //setenv("OCL_OUTPUT_BUILD_LOG","1",1);


	/* select platform or just show all of them*/
	if(argc > 1) {
		i = atoi(argv[1]);
		if(clutl_platform_select(i, &platform_id))
			fprintf(stderr, "clutl_platform_select error\n"), exit(1);
	}else{
		cnt = clutl_platform_select(-1, &platform_id);
		printf("=======================\n");
		printf("Please select platform:");
		i = fgetc(stdin) - '0';

		if(i < 1 || i> cnt ) exit(0);

		if(clutl_platform_select(i, &platform_id))
			fprintf(stderr, "clutl_platform_select error\n"), exit(1);
	}
	
	run_myGEMM(platform_id, argc > 2?argv[2]:"myGEMM1");
}





// Repeat all kernels multiple times to get an average timing result
#define NUM_RUNS 10

// Size of the matrices - K, M, N (squared)
#define SIZE (4096/4)

// Threadblock sizes (e.g. for kernels myGEMM1 or myGEMM2)
#define TS 32
void run_myGEMM(cl_platform_id platform_id, const char * kernel_filename)
{
    cl_device_id device_id = NULL;
    cl_program program = NULL;

    cl_uint ret_num_devices;
    cl_uint ret_num_platforms;
    cl_int ret;

    double tbase;

    // Set the sizes
    int K = SIZE;
    int M = SIZE;
    int N = SIZE;

    // Create the matrices and initialize them with random values
    float* A = (float*)malloc(M*K*sizeof(float*));
    float* B = (float*)malloc(K*N*sizeof(float*));
    float* C = (float*)malloc(M*N*sizeof(float*));
    float* D = (float*)malloc(M*N*sizeof(float*) + 1);
    for (int i=0; i<M*K; i++) { A[i] = 3.6*i + i*i + 3.1; }
    for (int i=0; i<K*N; i++) { B[i] = 1.2*i + 0.01*i*i + 13.9; }
    for (int i=0; i<M*N; i++) { C[i] = 0.0; }

    const char * cl_filename = "./myGEMM.cl";

	printf("run_myGEMM() with %s...start \n", cl_filename);
	printf("CPU version start ... \n");
	tbase = gettime_sec();

	if(load_bin("myGEMM.matD", D, (M*N + 1)*sizeof(float*)) <= 0){
		matmult(A,B,D,M,K,N);
		D[M*N] = (float)(gettime_sec() - tbase);
		save_bin("myGEMM.matD", D, (M*N + 1)*sizeof(float*));
	}

	printf("CPU version complete %.3f sec\n", D[M*N]);

    ret = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_DEFAULT, 1, &device_id, &ret_num_devices);

    clutl_device_caps(device_id);

    //clCreateSubDevices(device_id, CL_DEVICE_PARTITION_BY_COUNTS, )

    cl_context context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &ret); /* Create OpenCL context */
    cl_command_queue queue = clCreateCommandQueue(context, device_id, CL_QUEUE_PROFILING_ENABLE, &ret); /* Create Command Queue */

    if(clutl_build_program(context, device_id,	cl_filename, &program))
    	fprintf(stderr,"clutl_build_program error\n"),exit(1);
    //setenv("OCL_OUTPUT_ASM","1",0);
    //setenv("OCL_OUTPUT_BUILD_LOG","1",0);

    // Prepare OpenCL memory objects
    cl_mem bufA = clCreateBuffer(context, CL_MEM_READ_ONLY,  M*K*sizeof(float), NULL, NULL);
    cl_mem bufB = clCreateBuffer(context, CL_MEM_READ_ONLY,  K*N*sizeof(float), NULL, NULL);
    cl_mem bufC = clCreateBuffer(context, CL_MEM_READ_WRITE, M*N*sizeof(float), NULL, NULL);

    // Copy matrices to the GPU
    clEnqueueWriteBuffer(queue, bufA, CL_TRUE, 0, M*K*sizeof(float), A, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, bufB, CL_TRUE, 0, K*N*sizeof(float), B, 0, NULL, NULL);
    clEnqueueWriteBuffer(queue, bufC, CL_TRUE, 0, M*N*sizeof(float), C, 0, NULL, NULL);

    // Configure the myGEMM kernel and set its arguments
    cl_kernel kernel = clCreateKernel(program, kernel_filename, &ret);
    clutl_CheckError(ret);

    clSetKernelArg(kernel, 0, sizeof(int), (void*)&M);
    clSetKernelArg(kernel, 1, sizeof(int), (void*)&N);
    clSetKernelArg(kernel, 2, sizeof(int), (void*)&K);
    clSetKernelArg(kernel, 3, sizeof(cl_mem), (void*)&bufA);
    clSetKernelArg(kernel, 4, sizeof(cl_mem), (void*)&bufB);
    clSetKernelArg(kernel, 5, sizeof(cl_mem), (void*)&bufC);

    cl_double g_NDRangePureExecTimeNs = 0;
    // Start the timed loop
    printf(">>> Starting %d myGEMM runs...\n", NUM_RUNS);
    double starttime = gettime_sec();
    for (int r=0; r<NUM_RUNS; r++) {

        // Run the myGEMM kernel
        const size_t local[2] = { 16, 16 };
        const size_t global[2] = { M, N };
        cl_event event = NULL;
        cl_int err;
        err = clEnqueueNDRangeKernel(queue, kernel, 2, NULL, global, local, 0, NULL, &event);

        clutl_CheckError(err);

        // Wait for calculations to be finished
        clWaitForEvents(1, &event);

        cl_ulong start = 0, end = 0;
        clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
        clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);

        //END-START gives you hints on kind of “pure HW execution time”
        //the resolution of the events is 1e-09 sec
        g_NDRangePureExecTimeNs += (cl_double)(end - start);
    }

    // End the timed loop
    double endtime = gettime_sec();
    double runtime = (endtime - starttime) / (double)NUM_RUNS;

    double gflop = ((double)K * (double)M * (double)N * 2) / (1000*1000*1000);
    double gflop_add = ((double)M * (double)N) / (1000*1000*1000);
    printf(">>> Done. Host side: took %.3lf seconds per run, %.1lf GFLOPS (%.1lf GFLOPS for add)\n", runtime, gflop/runtime, gflop_add/runtime);
    printf(">>> Event Profiling: took %.3lf seconds per run\n", g_NDRangePureExecTimeNs*1e-9/(cl_double)NUM_RUNS);

    // Copy the output matrix C back to the CPU memory
    clEnqueueReadBuffer(queue, bufC, CL_TRUE, 0, M*N*sizeof(float), C, 0, NULL, NULL);

    int ecnt = matcmp(C, D, M, N);
    if(ecnt)
    	printf(">>> ***** %d(%d%%) errors were found in GPU result ***** \n", ecnt, ecnt*100/(M*N));

    // Free the OpenCL memory objects
    clReleaseMemObject(bufA);
    clReleaseMemObject(bufB);
    clReleaseMemObject(bufC);

    // Clean-up OpenCL
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
    clReleaseProgram(program);
    clReleaseKernel(kernel);

    // Free the host memory objects
    free(A);
    free(B);
    free(C);

	printf("run_myGEMM()...over \n");
}



/* first dimension is continuously allocated in memory
 * OR we say its stored column-by-column
 * A: M*K
 * B: K*N
 * C: M*N
 * */
void matmult(float *A, float *B, float * C, int M, int K, int N)
{
	for (int m=0; m<M; m++) {
		for (int n=0; n<N; n++) {
			float acc = 0.0f;
			for (int k=0; k<K; k++) {
				acc += A[k*M + m] * B[n*K + k];
			}
			C[n*M + m] = acc;
		}
	}
}

int matcmp(float *A, float *B, int M, int N)
{
	int ecnt = 0;
	for (int m=0; m<M; m++) {
		for (int n=0; n<N; n++) {
			if(A[m + n*M] != B[m + n*M])
				ecnt ++;
		}
	}
	return ecnt;
}

double gettime_sec(void)
{
    // Timers
    struct timeval Tvalue;
    struct timezone dummy;

	gettimeofday(&Tvalue, &dummy);

	return (double)Tvalue.tv_sec + 1.0e-6*((double)Tvalue.tv_usec);
}



char * load_src(const char *fileName)
{
#define MAX_SOURCE_SIZE (0x100000)
	char * source_str = NULL;
	FILE *fp = fopen(fileName, "r");
	size_t source_size = 0;
    if (!fp) {
		fprintf(stderr, "Failed to load kernel.\n");
		return NULL;
    }
    source_str = (char*)malloc(MAX_SOURCE_SIZE);
    source_size = fread(source_str, 1, MAX_SOURCE_SIZE, fp);
    source_str[source_size] = 0;
    fclose(fp);
    return source_str;
}
int load_bin(const char *filename, void * data, int size)
{
	int ret = 0;
	FILE *fp = fopen(filename, "rb");
	if(fp == NULL) return 0;
	ret = fread(data, 1, size, fp);
	fclose(fp);
	return ret;
}
int save_bin(const char *filename, void * data, int size)
{
	int ret = 0;
	FILE *fp = fopen(filename, "wb");
	if(fp == NULL) return 0;
	ret = fwrite(data, 1, size, fp);
	fclose(fp);
	return ret;
}
void show_runtime_map(void)
{
	char map_line[1024];
	FILE * fp = fopen("/proc/self/maps","rb");
	while(fgets(map_line, sizeof(map_line), fp))
		printf(map_line);
	fclose(fp);
}



/*
 * Given a cl code and return a string represenation
 */
const char* clutl_GetErrorString(int errorCode) {
    switch (errorCode) {
        case 0: return "CL_SUCCESS";
        case -1: return "CL_DEVICE_NOT_FOUND";
        case -2: return "CL_DEVICE_NOT_AVAILABLE";
        case -3: return "CL_COMPILER_NOT_AVAILABLE";
        case -4: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
        case -5: return "CL_OUT_OF_RESOURCES";
        case -6: return "CL_OUT_OF_HOST_MEMORY";
        case -7: return "CL_PROFILING_INFO_NOT_AVAILABLE";
        case -8: return "CL_MEM_COPY_OVERLAP";
        case -9: return "CL_IMAGE_FORMAT_MISMATCH";
        case -10: return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
        case -12: return "CL_MAP_FAILURE";
        case -13: return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
        case -14: return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
        case -15: return "CL_COMPILE_PROGRAM_FAILURE";
        case -16: return "CL_LINKER_NOT_AVAILABLE";
        case -17: return "CL_LINK_PROGRAM_FAILURE";
        case -18: return "CL_DEVICE_PARTITION_FAILED";
        case -19: return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";
        case -30: return "CL_INVALID_VALUE";
        case -31: return "CL_INVALID_DEVICE_TYPE";
        case -32: return "CL_INVALID_PLATFORM";
        case -33: return "CL_INVALID_DEVICE";
        case -34: return "CL_INVALID_CONTEXT";
        case -35: return "CL_INVALID_QUEUE_PROPERTIES";
        case -36: return "CL_INVALID_COMMAND_QUEUE";
        case -37: return "CL_INVALID_HOST_PTR";
        case -38: return "CL_INVALID_MEM_OBJECT";
        case -39: return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
        case -40: return "CL_INVALID_IMAGE_SIZE";
        case -41: return "CL_INVALID_SAMPLER";
        case -42: return "CL_INVALID_BINARY";
        case -43: return "CL_INVALID_BUILD_OPTIONS";
        case -44: return "CL_INVALID_PROGRAM";
        case -45: return "CL_INVALID_PROGRAM_EXECUTABLE";
        case -46: return "CL_INVALID_KERNEL_NAME";
        case -47: return "CL_INVALID_KERNEL_DEFINITION";
        case -48: return "CL_INVALID_KERNEL";
        case -49: return "CL_INVALID_ARG_INDEX";
        case -50: return "CL_INVALID_ARG_VALUE";
        case -51: return "CL_INVALID_ARG_SIZE";
        case -52: return "CL_INVALID_KERNEL_ARGS";
        case -53: return "CL_INVALID_WORK_DIMENSION";
        case -54: return "CL_INVALID_WORK_GROUP_SIZE";
        case -55: return "CL_INVALID_WORK_ITEM_SIZE";
        case -56: return "CL_INVALID_GLOBAL_OFFSET";
        case -57: return "CL_INVALID_EVENT_WAIT_LIST";
        case -58: return "CL_INVALID_EVENT";
        case -59: return "CL_INVALID_OPERATION";
        case -60: return "CL_INVALID_GL_OBJECT";
        case -61: return "CL_INVALID_BUFFER_SIZE";
        case -62: return "CL_INVALID_MIP_LEVEL";
        case -63: return "CL_INVALID_GLOBAL_WORK_SIZE";
        case -64: return "CL_INVALID_PROPERTY";
        case -65: return "CL_INVALID_IMAGE_DESCRIPTOR";
        case -66: return "CL_INVALID_COMPILER_OPTIONS";
        case -67: return "CL_INVALID_LINKER_OPTIONS";
        case -68: return "CL_INVALID_DEVICE_PARTITION_COUNT";
        case -69: return "CL_INVALID_PIPE_SIZE";
        case -70: return "CL_INVALID_DEVICE_QUEUE";
        case -71: return "CL_INVALID_SPEC_ID";
        case -72: return "CL_MAX_SIZE_RESTRICTION_EXCEEDED";
        case -1002: return "CL_INVALID_D3D10_DEVICE_KHR";
        case -1003: return "CL_INVALID_D3D10_RESOURCE_KHR";
        case -1004: return "CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR";
        case -1005: return "CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR";
        case -1006: return "CL_INVALID_D3D11_DEVICE_KHR";
        case -1007: return "CL_INVALID_D3D11_RESOURCE_KHR";
        case -1008: return "CL_D3D11_RESOURCE_ALREADY_ACQUIRED_KHR";
        case -1009: return "CL_D3D11_RESOURCE_NOT_ACQUIRED_KHR";
        case -1010: return "CL_INVALID_DX9_MEDIA_ADAPTER_KHR";
        case -1011: return "CL_INVALID_DX9_MEDIA_SURFACE_KHR";
        case -1012: return "CL_DX9_MEDIA_SURFACE_ALREADY_ACQUIRED_KHR";
        case -1013: return "CL_DX9_MEDIA_SURFACE_NOT_ACQUIRED_KHR";
        case -1093: return "CL_INVALID_EGL_OBJECT_KHR";
        case -1092: return "CL_EGL_RESOURCE_NOT_ACQUIRED_KHR";
        case -1001: return "CL_PLATFORM_NOT_FOUND_KHR";
        case -1057: return "CL_DEVICE_PARTITION_FAILED_EXT";
        case -1058: return "CL_INVALID_PARTITION_COUNT_EXT";
        case -1059: return "CL_INVALID_PARTITION_NAME_EXT";
        case -1094: return "CL_INVALID_ACCELERATOR_INTEL";
        case -1095: return "CL_INVALID_ACCELERATOR_TYPE_INTEL";
        case -1096: return "CL_INVALID_ACCELERATOR_DESCRIPTOR_INTEL";
        case -1097: return "CL_ACCELERATOR_TYPE_NOT_SUPPORTED_INTEL";
        case -1000: return "CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR";
        case -1098: return "CL_INVALID_VA_API_MEDIA_ADAPTER_INTEL";
        case -1099: return "CL_INVALID_VA_API_MEDIA_SURFACE_INTEL";
        case -1100: return "CL_VA_API_MEDIA_SURFACE_ALREADY_ACQUIRED_INTEL";
        case -1101: return "CL_VA_API_MEDIA_SURFACE_NOT_ACQUIRED_INTEL";
        default: return "CL_UNKNOWN_ERROR";
    }
}

void clutl_device_caps(cl_device_id device)
{
	size_t val_sz;
	char val[1024];
	size_t n;
	cl_uint wi_maxdim;
	size_t *wi_sz;
	cl_uint val_uint;
	cl_device_fp_config fpcfg;
	cl_ulong lc_memsz, gl_memsz;
	int rshift = 0;

#define PRINTINFO(name, fmt, rvalue) \
		clGetDeviceInfo(device, name, sizeof(*(rvalue)), rvalue, &n); \
		printf("%-40s: " fmt "\n",#name, *(rvalue));

#define PRINTINFO2(name, fmt, rvalue, rshift) \
		clGetDeviceInfo(device, name, sizeof(*(rvalue)), rvalue, &n); \
		printf("%-40s: " fmt "\n",#name, (*(rvalue) >> rshift));

	PRINTINFO(CL_DEVICE_NAME, "%s", &val);
	PRINTINFO(CL_DEVICE_MAX_COMPUTE_UNITS, "%u", &val_uint);
	PRINTINFO(CL_DEVICE_MAX_CLOCK_FREQUENCY, "%u", &val_uint);
	PRINTINFO(CL_DEVICE_ADDRESS_BITS, "%u", &val_uint);
	// work group size limit: 512(8EU * 7threads * SIMD-8)
	PRINTINFO(CL_DEVICE_MAX_WORK_GROUP_SIZE, "%u \t(upper limit of product(work_group_size))", &val_sz);
	PRINTINFO(CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, "%u \t(upper limit of dim(work_group_size))", &wi_maxdim);

	wi_sz = (size_t *)malloc(sizeof(size_t) * wi_maxdim);
	clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(size_t) * wi_maxdim, wi_sz, &n);
	printf("%-40s: ", "CL_DEVICE_MAX_WORK_ITEM_SIZES");
	for(int i=0;i<n/sizeof(size_t);i++)
		printf("%u ", wi_sz[i]);
	printf("\t(uper limit of each dim of work_group_size)\n");
	free(wi_sz);

	PRINTINFO2(CL_DEVICE_LOCAL_MEM_SIZE, "%6lu KB  (Gen9 should have 64KB shared local mem)", &lc_memsz, 10)
	PRINTINFO2(CL_DEVICE_GLOBAL_MEM_SIZE, "%6lu MB  (Limited only by GPU's address bits)", &gl_memsz, 20)

	PRINTINFO(CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT, "%u (SIMD)", &val_uint);
	PRINTINFO(CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE, "%u (SIMD)", &val_uint);
	PRINTINFO(CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT, "%u (SIMD)", &val_uint);
	PRINTINFO(CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT, "%u (SIMD)", &val_uint);
	PRINTINFO(CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR, "%u (SIMD)", &val_uint);

	PRINTINFO(CL_DEVICE_EXTENSIONS, "%s", &val);
}

		
int
clutl_build_program(
		cl_context context,
		cl_device_id device,
		const char * filename,
		cl_program *ptr_program)
{
	// Compile the kernel
	const char * kernelstring = load_src(filename);
	if(kernelstring == NULL){
		fprintf(stderr, ">>> clutl_build_kernel: load_src(%s) error\n", filename);
		return 1;
	}

	cl_program program = clCreateProgramWithSource(context, 1, &kernelstring, NULL, NULL);
	clBuildProgram(program, 0, NULL, "", NULL, NULL);

	free((void*)kernelstring);

	// Check for compilation errors
	size_t logSize;
	clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize);
	char* messages = (char*)malloc((1+logSize)*sizeof(char));
	clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, logSize, messages, NULL);
	messages[logSize] = '\0';
	if (logSize > 10) { fprintf(stderr, ">>> Compiler message: %s\n", messages); }
	free(messages);

	*ptr_program = program;
	return 0;
}

int
clutl_platform_select(
		int select_id,
		cl_platform_id * ptr_platform_id)
{
	cl_int ret;
	cl_uint ret_num_platforms;
	cl_platform_id platform_id[32]={0};
	cl_uint i,j;
	char *info;
	size_t infoSize;
#define ID_NAME(id) {id, #id}
	struct attr_tag{
		cl_platform_info id;
		const char * name;
	}attrs[]={
		ID_NAME(CL_PLATFORM_NAME),
		ID_NAME(CL_PLATFORM_VENDOR),
		ID_NAME(CL_PLATFORM_VERSION),
		ID_NAME(CL_PLATFORM_PROFILE),
		ID_NAME(CL_PLATFORM_EXTENSIONS),
	};

	const char * ext_func[]={
			"clCreateImageFromFdINTEL",
			"clCreateBufferFromFdINTEL",
			"clGetDeviceIDsFromVA_APIMediaAdapterINTEL",
			"clCreateFromVA_APIMediaSurfaceINTEL",
			"clEnqueueAcquireVA_APIMediaSurfacesINTEL",
			"clEnqueueReleaseVA_APIMediaSurfacesINTEL"
	};
#define cntof(x) sizeof(x)/sizeof(x[0])

    /* Get Platform and Device Info */
    clGetPlatformIDs(sizeof(platform_id)/sizeof(platform_id[0]),
    				platform_id,
					&ret_num_platforms);

	printf("Total %d Platforms:\n", ret_num_platforms);
	for(i=0; i<ret_num_platforms; i++) {
		if(select_id == (i+1) || select_id <= 0){
			printf("PLATFORM %u: %s\n", i+1, select_id == (i+1)?"is selected":"");
			for (j = 0; j < cntof(attrs); j++) {
				clGetPlatformInfo(platform_id[i], attrs[j].id, 0, NULL, &infoSize);
				info = (char*) malloc(infoSize);
				clGetPlatformInfo(platform_id[i], attrs[j].id, infoSize, info, &infoSize);
				printf("\t%-23s : %s\n",attrs[j].name, info);
				free(info);
			}

			for (j = 0; j < cntof(ext_func); j++){
				void * pfunc = clGetExtensionFunctionAddressForPlatform(platform_id[i], ext_func[j]);
				printf("\tEXTENSION: %s%s = %p\n"ANSI_COLOR_RESET,
						pfunc?ANSI_COLOR_GREEN:ANSI_COLOR_RED,
						ext_func[j],
						pfunc);
			}
		}
	}

    printf("\n\n");

	if(select_id > 0){
		*ptr_platform_id = platform_id[select_id - 1];
		return 0;
	}else{
		*ptr_platform_id = 0;
		return ret_num_platforms;
	}
}
