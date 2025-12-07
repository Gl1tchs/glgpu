[[vk::binding(0, 0)]] 
RWStructuredBuffer<float> buffer_data;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint index = dispatchThreadID.x;
    
    float value = buffer_data[index];
    buffer_data[index] = value * value;
}